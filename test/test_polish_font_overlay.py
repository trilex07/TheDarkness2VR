"""Official patch shadowing and four-file transactional installation."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from polish_font_overlay import overlay_header, check_lookup
from polish_fonts import PL_LETTERS
import polish_package_install as installer


class FontOverlayTests(unittest.TestCase):
    def test_preserves_official_trailing_metadata(self):
        old=bytes(160);new=bytearray(old)
        struct.pack_into('<I',new,8,129);struct.pack_into('<I',new,40,129)
        dlc=old+b'official extra metadata'
        fixed=overlay_header(old,bytes(new),dlc,8)
        self.assertEqual(fixed,bytes(new)+b'official extra metadata')

    def test_rejects_unreconciled_base_header_change(self):
        old=bytes(160);new=bytearray(old);new[150]=1
        self.assertRaises(ValueError,overlay_header,old,bytes(new),old,8)

    def test_rejects_different_official_count_contract(self):
        old=bytes(160);dlc=bytearray(old);dlc[8]=1
        self.assertRaises(ValueError,overlay_header,old,old,bytes(dlc),8)

    def test_patch_layer_shadows_polish_base_until_repaired(self):
        codes=[42,65,79]+[x[1] for x in PL_LETTERS];codes.sort()
        n=len(codes);header=bytearray(100);struct.pack_into('<I',header,0,n)
        body=bytes(n*44)+struct.pack('<'+str(n)+'H',*codes)
        self.assertEqual(check_lookup(header,body,0,[42,65,79]),n)
        # Native DLC first wins: English-only override cannot satisfy Polish.
        old=bytearray(header);struct.pack_into('<I',old,0,3)
        self.assertRaises(ValueError,check_lookup,old,bytes(3*44)+struct.pack('<3H',42,65,79),0,[42,65,79])

    def package(self,root):
        package=root/'package';(package/'payload').mkdir(parents=True)
        cache=root/'patch';cache.mkdir();base=root/'base';base.mkdir()
        before={};after={}
        for n in installer.FONT_OVERLAY_NAMES:
            (cache/n).write_bytes(('old '+n).encode());(package/'payload'/n).write_bytes(('new '+n).encode())
            before[n]=installer.sha(cache/n);after[n]=installer.sha(package/'payload'/n)
        for n in installer.BASE_FONT_NAMES:(base/n).write_bytes(('base '+n).encode())
        m=dict(kind='dlc-font-overlay-v1',files=after,preimage_profiles={'official':before},
               base_font_files={n:installer.sha(base/n) for n in installer.BASE_FONT_NAMES})
        (package/'manifest.json').write_text(json.dumps(m))
        (package/'SHA256SUMS').write_text(''.join(installer.sha(p)+'  '+p.relative_to(package).as_posix()+'\n'
            for p in sorted(package.rglob('*')) if p.is_file()))
        return package,cache,base,before,after

    def test_four_file_install_and_verified_rollback(self):
        with tempfile.TemporaryDirectory() as d, patch.object(installer,'game_free'):
            root=Path(d);pkg,cache,base,before,after=self.package(root)
            self.assertEqual(installer.install(pkg,cache,root/'backup',base)['status'],'INSTALLED_VERIFIED')
            self.assertEqual({n:installer.sha(cache/n) for n in before},after)
            self.assertEqual(installer.rollback(root/'backup',cache)['status'],'ROLLBACK_VERIFIED')
            self.assertEqual({n:installer.sha(cache/n) for n in before},before)

    def test_mismatched_base_aborts_without_any_writes(self):
        with tempfile.TemporaryDirectory() as d, patch.object(installer,'game_free'):
            root=Path(d);pkg,cache,base,before,_=self.package(root)
            (base/'H.Font.cache').write_bytes(b'other candidate')
            self.assertRaises(ValueError,installer.install,pkg,cache,root/'backup',base)
            self.assertFalse((root/'backup').exists())
            self.assertEqual({n:installer.sha(cache/n) for n in before},before)

    def test_unknown_package_kind_is_rejected(self):
        self.assertRaises(ValueError,installer.payload_names,{'kind':'arbitrary-files'})

if __name__=='__main__':unittest.main()
