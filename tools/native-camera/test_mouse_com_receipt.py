"""CPU-only existing S21 fixture plus adversarial COM/module/identity controls."""
import copy
from pathlib import Path
import struct
import time
import unittest
from unittest.mock import patch
import mouse_com_receipt as r

base = r.load_inspector()
from test_flash_native_receipt import Fixture


class ComTests(unittest.TestCase):
    def setUp(self):
        self.f = Fixture()
        self.hwnd, self.device, self.table = 0x123456, 0x3000000, 0x3010000
        self.module, self.target = 0x65000000, 0x65001235  # valid UNALIGNED code
        self.f.word(self.f.mouse+0x1E8, self.hwnd)
        self.f.put(self.f.mouse+7, b'\1')
        self.f.word(self.f.mouse+0x1A4, self.device)
        self.f.word(self.device, self.table)
        self.f.put(self.table, struct.pack('<11I', *([self.target]*11)))
        dos = bytearray(64);dos[:2] = b'MZ';struct.pack_into('<I', dos, 60, 128)
        self.f.put(self.module, dos)
        header = bytearray(24);header[:4] = b'PE\0\0';struct.pack_into('<HHI', header, 4, 0x14C, 1, 123)
        struct.pack_into('<H', header, 20, 224);self.f.put(self.module+128, header)
        tail = bytearray(264);struct.pack_into('<H', tail, 0, 0x10B);struct.pack_into('<I', tail, 56, 0x3000)
        struct.pack_into('<III', tail, 232, 0x1000, 0x1000, 0x1000)
        struct.pack_into('<I', tail, 260, 0x60000020);self.f.put(self.module+152, tail)
        self.f.put(self.target-5, bytes(range(101)))
        self.page = dict(allocation_base=self.module, region_base=self.module+0x1000,
                         region_size=0x1000, state=0x1000, type=0x1000000,
                         protect=0x20, mapped_path=r'\Device\Synthetic\external.dll')
        self.trace = {}

    def collect(self, read=None, identity=None, guard=lambda: None, query=None, clock=lambda: 0):
        return r.collect(base, read or self.f.read, self.f.pe,
                         identity or (lambda: dict(hwnd=self.hwnd)), guard,
                         query or (lambda _: self.page.copy()), self.trace, clock)

    def test_actual_chain_two_equal_with_unaligned_code_and_raw_headers(self):
        result = self.collect()
        self.assertEqual(result['status'], 'READ_ONLY_EQUAL_PAIR')
        self.assertEqual(result['snapshot']['device'], self.device)
        method = result['snapshot']['method']
        self.assertEqual(method['target'], self.target)
        self.assertEqual(bytes.fromhex(method['method_span_hex']), bytes(range(101)))
        self.assertEqual(len(self.trace['snapshots']), 2)
        self.assertFalse(result['lifetime_lease']);self.assertFalse(result['hook_admitted'])
        self.assertLess(result['read_bytes'], 131072)

    def test_wrong_mouse_window(self):
        self.f.word(self.f.mouse+0x1E8, 7)
        with self.assertRaisesRegex(ValueError, 'HWND'):self.collect()
        self.assertEqual(len(self.trace['snapshots']), 1)

    def test_bad_device_or_table_pointer(self):
        for address in (self.f.mouse+0x1A4, self.device):
            old = self.f.read(address, 4);self.f.word(address, 1)
            with self.assertRaisesRegex(ValueError, 'pointer'):self.collect()
            self.f.put(address, old)

    def test_non_image_guarded_non_executable_or_foreign_allocation(self):
        for key, value in [('type', 0x20000), ('state', 0x2000), ('protect', 4), ('protect', 0x120)]:
            with self.subTest(key=key, value=value):
                page = dict(self.page, **{key: value})
                with self.assertRaisesRegex(ValueError, 'executable image'):self.collect(query=lambda _: page)
        calls = 0
        def query(_):
            nonlocal calls
            calls += 1
            return dict(self.page, allocation_base=self.module if calls == 1 else self.module+0x1000)
        with self.assertRaisesRegex(ValueError, 'one committed'):self.collect(query=query)

    def test_invalid_pe_signature_architecture_extent_or_section(self):
        for address, data in [(self.module, b'XX'), (self.module+60, struct.pack('<I', 4097)),
                              (self.module+132, b'\x64\x86'), (self.module+152, b'\x0b\x02'),
                              (self.module+208, b'\0'*4), (self.module+412, b'\0'*4)]:
            with self.subTest(address=address):
                old = self.f.read(address, len(data));self.f.put(address, data)
                with self.assertRaises(ValueError):self.collect()
                self.f.put(address, old)

    def test_target_or_method_bytes_change_preserves_raw_refusal_rows(self):
        calls = 0
        def read(a,n):
            nonlocal calls
            if a == self.table:
                calls += 1
                if calls == 2:self.f.word(self.table+0x28, self.target+1)
            return self.f.read(a,n)
        with self.assertRaises(ValueError):self.collect(read=read)
        self.assertEqual(len(self.trace['snapshots']), 2)
        self.assertNotEqual(self.trace['snapshots'][0]['get_data'], self.trace['snapshots'][1]['get_data'])

    def test_method_mutation_between_reads(self):
        calls = 0
        def read(a,n):
            nonlocal calls
            if a == self.target-5:
                calls += 1
                if calls == 2:self.f.put(a, b'\x99'+bytes(range(1,101)))
            return self.f.read(a,n)
        with self.assertRaisesRegex(ValueError, 'unequal'):self.collect(read=read)

    def test_reused_pid_identity_or_revoked_owner(self):
        count = 0
        def identity():
            nonlocal count
            count += 1
            return dict(hwnd=self.hwnd, created_ticks=count)
        with self.assertRaisesRegex(ValueError, 'identity'):self.collect(identity=identity)
        def guard():raise ValueError('revoked owner')
        with self.assertRaisesRegex(ValueError, 'revoked'):self.collect(guard=guard)

    def test_read_failure_deadline_and_changed_loaded_method(self):
        with self.assertRaises(ValueError):self.collect(read=lambda a,n:b'')
        ticks = iter([0,3])
        with self.assertRaisesRegex(ValueError, 'budget'):self.collect(clock=lambda:next(ticks))
        target = r.METHODS[0][0]
        def read(a,n):
            b = self.f.read(a,n)
            return bytes([b[0]^1])+b[1:] if a == target else b
        with self.assertRaisesRegex(ValueError, 'loaded method mismatch: mouse_position'):self.collect(read=read)


class CliIdentityTests(unittest.TestCase):
    def setUp(self):
        self.e = dict(pid=42, hwnd=16, created_ticks=123, exe='EXACT.exe',
                      token='CURRENT', session_id='current', deadline=time.time()+60)

    def test_inert_and_complete_cli_parser(self):
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('no file access')):
            self.assertEqual(r.main([]), 0)
            self.assertEqual(r.main(['--expected', 'unused', '--owner-name', 'darkness2', '--output', 'unused']), 0)
        with self.assertRaises(SystemExit):r.main(['--live'])

    def test_exact_owner_token_file_deadline(self):
        r.check_owner(self.e, 'main', dict(owner='main', token='CURRENT'), b'same', b'same')
        for lease, actual in [(dict(owner='other', token='CURRENT'), b'same'),
                              (dict(owner='main', token='OLD'), b'same'),
                              (dict(owner='main', token='CURRENT'), b'changed')]:
            with self.assertRaises(ValueError):r.check_owner(self.e, 'main', lease, actual, b'same')
        self.e['deadline'] = time.time()-1
        with self.assertRaises(ValueError):r.validate_expected(self.e)

    def test_expected_identity_validation(self):
        for key, value in [('pid', True), ('created_ticks', 0), ('token', ''), ('session_id', ' '), ('deadline', float('nan'))]:
            bad = dict(self.e, **{key:value})
            with self.assertRaises(ValueError):r.validate_expected(bad)

    def test_actual_live_identity_predicate(self):
        observed = dict(pid=42, thread=77, exit_code=259, created_ticks=123,
                        exe='EXACT.exe', title='The Darkness II', foreground=16)
        r.check_identity(self.e, observed)
        for key, value in [('pid', 43), ('thread', 0), ('exit_code', 0), ('created_ticks', 124),
                           ('exe', 'OTHER.exe'), ('title', 'Preview'), ('foreground', 17)]:
            with self.assertRaises(ValueError):r.check_identity(self.e, dict(observed, **{key:value}))


if __name__ == '__main__':unittest.main()
