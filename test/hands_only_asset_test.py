"""Asset transaction regressions with generated data, never shipping stock bytes."""
import contextlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('hands_only_asset', Path(__file__).parents[1] / 'tools/hands_only_asset.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
HANDS = [(0, 11106), (14550, 8070), (25461, 6012), (33459, 4485), (39276, 2736)]
SLEEVES = [(11106, 3444), (22620, 2841), (31473, 1986), (37944, 1332), (42012, 897)]


def fixtures():
    header = b'/D2/Characters/Jackie/JackieFPArmsHandRevised\0/D2/Characters/Jackie/JackieFPArmsSleeveRevised\0'
    for label, ranges in [(b'/HandRevised1', HANDS), (b'/SleeveRevised', SLEEVES)]:
        header += struct.pack('<I', len(label)) + label
        header += struct.pack('<10I', *[x[0] for x in ranges], *[x[1] for x in ranges])
    body = bytes((i * 17) % 256 for i in range(m.IB_OFFSET))
    body += struct.pack('<' + 'H' * m.INDEX_COUNT, *[(i * 7) % 2860 for i in range(m.INDEX_COUNT)])
    return header, body


def toc_fixture():
    data = bytes.fromhex('4ec66718') + struct.pack('<I', 1)
    for parent, name in [(0, 'D2'), (1, 'Characters'), (2, 'Camera')]:
        data += struct.pack('<QQIIII64s', 2**64-1, 0, 0, 0, 0, parent, name.encode())
    data += struct.pack('<QQIIII64s', 128, 1, 512, 206112, 0, 3, b'FPJackieArms_skel.fbx')
    return data


class HandsOnly(unittest.TestCase):
    def setUp(self):
        self.header, self.body = fixtures()
        self.scope = contextlib.ExitStack()
        self.scope.enter_context(patch.object(m, 'HEADER_SHA', m.sha(self.header)))
        self.scope.enter_context(patch.object(m, 'BODY_SHA', m.sha(self.body)))
        self.addCleanup(self.scope.close)

    def test_all_lods_exact_hand_vertex_and_texture_preservation(self):
        output, hands, sleeves = m.patch(self.header, self.body)
        self.assertEqual(hands, HANDS); self.assertEqual(sleeves, SLEEVES)
        self.assertEqual(output[:m.IB_OFFSET], self.body[:m.IB_OFFSET])
        self.assertEqual(len(output), len(self.body))
        for start, count in hands:
            a, b = m.IB_OFFSET + start * 2, m.IB_OFFSET + (start + count) * 2
            self.assertEqual(output[a:b], self.body[a:b])
        for start, count in sleeves:
            for i in range(start, start + count, 3):
                at = m.IB_OFFSET + i * 2
                tri = struct.unpack_from('<3H', output, at)
                self.assertEqual(tri, (struct.unpack_from('<H', self.body, at)[0],) * 3)

    def test_reject_foreign_assets_and_repatch(self):
        for h, b in [(self.header+b'X', self.body), (self.header, self.body[:-1]),
                     (self.header, m.patch(self.header, self.body)[0])]:
            with self.assertRaisesRegex(ValueError, 'preimage'):
                m.patch(h, b)

    def setup_install(self, root):
        cache = root / 'game' / 'Cache.Windows'; cache.mkdir(parents=True)
        exe = b'fixture only'; (cache.parent / 'DarknessII.exe').write_bytes(exe)
        blob = bytes(range(256)) * 4
        (cache / 'B.Misc.cache').write_bytes(blob)
        (cache / 'B.Misc.toc').write_bytes(toc_fixture())
        package = root / 'package'
        h, b = root / 'header', root / 'body'; h.write_bytes(self.header); b.write_bytes(self.body)
        m.prepare(h, b, package)
        for name, value in [('EXE_SHA', m.sha(exe)), ('ENTRY_OFFSET', 128), ('ENTRY_SIZE', 512),
                            ('ENTRY_SHA', m.sha(blob[128:640]))]:
            self.scope.enter_context(patch.object(m, name, value))
        self.scope.enter_context(patch.object(m, 'game_closed'))
        return cache, package, blob

    def test_install_extract_rollback_exact_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); cache, package, original = self.setup_install(root)
            backup = root / 'backup'
            m.install(cache, package, backup)
            data = (cache / 'B.Misc.cache').read_bytes()
            self.assertEqual(data[:len(original)], original)
            _, off, zsize, size, pad = m.toc_entry((cache / 'B.Misc.toc').read_bytes())
            self.assertEqual((off, zsize, size, pad), (len(original), len(self.body), len(self.body), 0))
            self.assertEqual(data[off:off+size], m.patch(self.header, self.body)[0])
            with self.assertRaises(ValueError): m.install(cache, package, root / 'again')
            m.rollback(backup / 'receipt.json'); m.rollback(backup / 'receipt.json')
            self.assertEqual((cache / 'B.Misc.cache').read_bytes(), original)
            self.assertEqual((cache / 'B.Misc.toc').read_bytes(), toc_fixture())

    def test_failed_toc_replace_recoverable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); cache, package, original = self.setup_install(root)
            with patch.object(m.os, 'replace', side_effect=OSError('injected')):
                with self.assertRaises(OSError): m.install(cache, package, root / 'backup')
            self.assertEqual((cache / 'B.Misc.toc').read_bytes(), toc_fixture())
            m.rollback(root / 'backup' / 'receipt.json')
            self.assertEqual((cache / 'B.Misc.cache').read_bytes(), original)

    def test_concurrent_mutation_blocks_rollback_before_any_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); cache, package, original = self.setup_install(root)
            m.install(cache, package, root / 'backup')
            path = cache / 'B.Misc.cache'; path.write_bytes(path.read_bytes() + b'foreign')
            toc_before = (cache / 'B.Misc.toc').read_bytes()
            with self.assertRaisesRegex(ValueError, 'Cache changed'):
                m.rollback(root / 'backup' / 'receipt.json')
            self.assertEqual((cache / 'B.Misc.toc').read_bytes(), toc_before)
            self.assertTrue(path.read_bytes().endswith(b'foreign'))

    def test_partial_append_recovery_preserves_original_prefix(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); cache, package, original = self.setup_install(root)
            with patch.object(m.os, 'replace', side_effect=OSError('injected')):
                with self.assertRaises(OSError): m.install(cache, package, root / 'backup')
            path = cache / 'B.Misc.cache'
            with path.open('r+b') as f: f.truncate(len(original) + 17)
            m.rollback(root / 'backup' / 'receipt.json')
            self.assertEqual(path.read_bytes(), original)


if __name__ == '__main__': unittest.main()
