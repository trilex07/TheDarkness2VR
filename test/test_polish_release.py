"""Safety regressions for the actual private package installer/formatter."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import polish_package_install as installer
from polish_translate import assemble


class PolishReleaseTests(unittest.TestCase):
    def test_deployed_preimage_uses_receipt_not_mutable_staging(self):
        from polish_release import deployed_preimage
        with tempfile.TemporaryDirectory() as folder:
            receipt=Path(folder)/'receipt.json'
            receipt.write_text(json.dumps({'files':[{'name':'B.Font.cache','afterSha256':'b'*64}]}))
            baseline={'B.Font.cache':'a'*64,'B.Texture_en.cache':'c'*64}
            self.assertEqual(deployed_preimage(baseline,receipt),{'B.Font.cache':'b'*64,'B.Texture_en.cache':'c'*64})
            self.assertEqual(baseline['B.Font.cache'],'a'*64)
            receipt.write_text(json.dumps({'files':[{'name':'unknown','afterSha256':'b'*64}]}))
            self.assertRaises(ValueError,deployed_preimage,baseline,receipt)

    def package(self,root):
        package=root/'package';(package/'payload').mkdir(parents=True)
        cache=root/'cache';cache.mkdir();old={};new={}
        for name in installer.NAMES:
            (cache/name).write_bytes(('original '+name).encode())
            (package/'payload'/name).write_bytes(('polski '+name).encode())
            old[name]=installer.sha(cache/name);new[name]=installer.sha(package/'payload'/name)
        (package/'manifest.json').write_text(json.dumps(dict(files=new,preimage_profiles={'known':old})))
        (package/'SHA256SUMS').write_text(''.join(installer.sha(p)+'  '+p.relative_to(package).as_posix()+'\n' for p in sorted(package.rglob('*')) if p.is_file()))
        return package,cache,old,new

    def font_package(self, root):
        package, cache, old, new = self.package(root)
        manifest_path = package/'manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['source_commit']='fixture-source'
        manifest['fonts']=[dict(font='font'+str(i),status='BINARY_AND_GLYPH_PASS',glyphs=[{}]*18) for i in range(6)]
        manifest_path.write_text(json.dumps(manifest))
        (package/'SHA256SUMS').write_text(''.join(installer.sha(p)+'  '+p.relative_to(package).as_posix()+'\n' for p in sorted(package.rglob('*')) if p.is_file() and p.name!='SHA256SUMS'))
        receipt=root/'rollback.json';receipt.write_text(json.dumps(dict(original=old)))
        return package,cache,receipt,old,new

    def test_font_only_release_preserves_accepted_speech_and_text(self):
        from polish_release import font_only_release
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);package,cache,receipt,old,new=self.font_package(root)
            result=font_only_release(package,cache,receipt,root/'font-only')
            checked=installer.verify(root/'font-only')
            self.assertEqual(result,checked)
            self.assertEqual(len(result['preserved_files']),8)
            for name in installer.NAMES:
                expected=old[name] if name in result['preserved_files'] else new[name]
                self.assertEqual(installer.sha(root/'font-only'/'payload'/name),expected)
            self.assertEqual({n:installer.sha(cache/n) for n in old},old)

    def test_font_only_package_is_reproducible_and_rollback_protected(self):
        from polish_release import font_only_release
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,receipt,old,new=self.font_package(root)
            font_only_release(package,cache,receipt,root/'first')
            font_only_release(package,cache,receipt,root/'second')
            a={p.relative_to(root/'first').as_posix():installer.sha(p) for p in (root/'first').rglob('*') if p.is_file()}
            b={p.relative_to(root/'second').as_posix():installer.sha(p) for p in (root/'second').rglob('*') if p.is_file()}
            self.assertEqual(a,b)
            self.assertIn('tools/polish_package_install.py',a)
            installed=installer.install(root/'first',cache,root/'backup')
            self.assertEqual(installed['status'],'INSTALLED_VERIFIED')
            for name in ('H.Misc.cache','H.Misc_en.cache','B.Misc_en.cache','F.Misc_en.cache'):
                self.assertEqual(installer.sha(cache/name),old[name])
            installer.rollback(root/'backup',cache)
            self.assertEqual({n:installer.sha(cache/n) for n in old},old)

    def test_font_only_release_rejects_changed_or_mixed_accepted_cache(self):
        from polish_release import font_only_release
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);package,cache,receipt,old,new=self.font_package(root)
            (cache/'H.Misc_en.cache').write_bytes(b'unexpected speech')
            self.assertRaises(ValueError,font_only_release,package,cache,receipt,root/'out')
            self.assertFalse((root/'out').exists())

    def test_font_only_release_rejects_tampered_source_without_output(self):
        from polish_release import font_only_release
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);package,cache,receipt,old,new=self.font_package(root)
            (package/'payload'/'B.Font.cache').write_bytes(b'corruption')
            self.assertRaises(ValueError,font_only_release,package,cache,receipt,root/'out')
            self.assertFalse((root/'out').exists())

    def test_install_then_rollback_restores_every_original_hash(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,old,new=self.package(root)
            self.assertEqual(installer.install(package,cache,root/'backup')['status'],'INSTALLED_VERIFIED')
            self.assertEqual({n:installer.sha(cache/n) for n in old},new)
            self.assertEqual(installer.rollback(root/'backup',cache)['status'],'ROLLBACK_VERIFIED')
            self.assertEqual({n:installer.sha(cache/n) for n in old},old)

    def test_unknown_preimage_and_tampered_payload_change_nothing(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,old,new=self.package(root);name=sorted(old)[0]
            (package/'payload'/name).write_bytes(b'corrupt')
            self.assertRaises(ValueError,installer.install,package,cache,root/'backup')
            self.assertEqual({n:installer.sha(cache/n) for n in old},old)
            self.assertFalse((root/'backup').exists())

    def test_mid_transaction_failure_rolls_back_all_written_files(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,old,new=self.package(root);replace=installer.os.replace;calls=0
            def fail_once(source,target):
                nonlocal calls
                calls+=1
                if calls==5:raise PermissionError('simulated file lock')
                return replace(source,target)
            with patch.object(installer.os,'replace',side_effect=fail_once):
                self.assertRaises(PermissionError,installer.install,package,cache,root/'backup')
            self.assertEqual({n:installer.sha(cache/n) for n in old},old)
            self.assertEqual(json.loads((root/'backup/rollback.json').read_text())['status'],'INSTALL_FAILED_ROLLBACK_VERIFIED')

    def test_unknown_preimage_is_rejected_before_backup_or_writes(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,old,new=self.package(root);name=sorted(old)[0]
            (cache/name).write_bytes(b'unknown user version')
            before={n:installer.sha(cache/n) for n in old}
            self.assertRaises(ValueError,installer.install,package,cache,root/'backup')
            self.assertEqual({n:installer.sha(cache/n) for n in old},before)
            self.assertFalse((root/'backup').exists())

    def test_changed_game_file_prevents_destructive_rollback(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(installer,'game_free'):
            root=Path(folder);package,cache,old,new=self.package(root)
            installer.install(package,cache,root/'backup');name=sorted(old)[0];(cache/name).write_bytes(b'new user change')
            self.assertRaises(ValueError,installer.rollback,root/'backup',cache)
            self.assertEqual((cache/name).read_bytes(),b'new user change')

    def test_native_formatting_is_protected_from_model_output(self):
        source='  <ACTION>Load %s: {count}\n[BUTTON_A] %1 %%  '
        self.assertEqual(assemble(source,{'Load':'Wczytaj'}),'  <ACTION>Wczytaj %s: {count}\n[BUTTON_A] %1 %%  ')
        self.assertRaises(ValueError,assemble,'Load %s',{'Load':'Zgubione %d'})


if __name__=='__main__':unittest.main()
