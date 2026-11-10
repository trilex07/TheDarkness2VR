"""ONE read-only mouse COM receipt for MAIN; inert unless --live.

Reuses unchanged S21 inspector for bounded reads, loaded contracts and native
movie/input/mouse chain. No target calls, hooks, writes, input, focus or launch.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import time

INSPECTOR = Path('C:/Users/Trilex/Darkness2VR/.unattended-audit-20261110/tools/native-camera/flash_native_receipt.py')
INSPECTOR_SHA = '639C0E8AE64107A531408ACAB49441A96DB4D3FE4E8AE06E1FF7D2F7F603C822'
METHODS = (
    (0x603190, 0x1B, 'e9c127f7d17c85159bc87d51c7aec652cb00fec8ab9114a7072931508bb045f2'),
    (0x43E160, 0x17, 'ace6e45967527155cce8426c6b949ac6dbd9e4e37d4eeb0b6dbc7b27e6418764'),
    (0x9A5D90, 0x17A, '67e02e71a4e562cd5c8ce6e55708d4e1c3f15e65495187ffe9be3724cc170515'),
    (0x821400, 0x79, 'abd98e7652244d6eb61ebac0a5f879044829789a86845f257631fca3f07511a5'),
)


def load_inspector():
    if hashlib.sha256(INSPECTOR.read_bytes()).hexdigest().upper() != INSPECTOR_SHA:
        raise ValueError('unchanged S21 inspector differs')
    sys.path.insert(0, str(INSPECTOR.parent))
    spec = importlib.util.spec_from_file_location('d2vr_s21_readonly', INSPECTOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def module_receipt(read, query, target):
    # Function addresses need NOT be DWORD-aligned. Only objects/tables do.
    if type(target) is not int or not 0x10005 <= target <= 0xFFFFFF9F:
        raise ValueError('invalid code address')
    pages = [query(target-5), query(target+95)]
    first = pages[0]
    base = first['allocation_base']
    for p in pages:
        if (p['allocation_base'] != base or p['mapped_path'] != first['mapped_path'] or
                p['state'] != 0x1000 or p['type'] != 0x1000000 or
                not p['protect'] & 0xF0 or p['protect'] & 0x101):
            raise ValueError('method span is not one committed executable image')
    if type(base) is not int or not 0x10000 <= base <= target or not first['mapped_path']:
        raise ValueError('invalid module identity')
    dos = read(base, 64)
    offset = struct.unpack_from('<I', dos, 60)[0]
    if dos[:2] != b'MZ' or not 64 <= offset <= 4096:
        raise ValueError('unsupported bounded DOS header')
    header = read(base+offset, 24)
    machine, sections = struct.unpack_from('<HH', header, 4)
    optional_size = struct.unpack_from('<H', header, 20)[0]
    if header[:4] != b'PE\0\0' or machine != 0x14C or not 1 <= sections <= 96 or not 96 <= optional_size <= 512:
        raise ValueError('unsupported bounded x86 PE header')
    tail = read(base+offset+24, optional_size+40*sections)
    image_size = struct.unpack_from('<I', tail, 56)[0]
    rva = target-base
    if struct.unpack_from('<H', tail)[0] != 0x10B or not 0 < image_size <= 0x40000000 or base+image_size > 0x100000000 or rva+96 > image_size:
        raise ValueError('invalid PE32 image extent')
    executable = False
    for i in range(sections):
        start = optional_size+40*i
        virtual_size, section_rva, raw_size = struct.unpack_from('<III', tail, start+8)
        flags = struct.unpack_from('<I', tail, start+36)[0]
        executable |= bool(flags & 0x20000000 and section_rva <= rva and rva+96 <= section_rva+max(virtual_size, raw_size))
    if not executable:
        raise ValueError('method span outside executable PE section')
    code = read(target-5, 101)
    return dict(target=target, module_base=base, method_rva=rva,
                mapped_path=first['mapped_path'], pages=pages,
                pe_timestamp=struct.unpack_from('<I', header, 8)[0], image_size=image_size,
                dos_hex=dos.hex(), nt_offset=offset, nt_and_sections_hex=(header+tail).hex(),
                method_prefix_bytes=5, method_bytes=96, method_span_hex=code.hex(),
                method_sha256=hashlib.sha256(code[5:]).hexdigest(),
                hook_admitted=False)


def collect(base, read, pe, identity, guard, query, trace, clock=time.monotonic):
    guard()
    owner = identity()
    bounded = base.BudgetRead(read, clock)  # unchanged512 calls/128KiB/2s

    def contracts():
        base.loaded_contract(bounded, pe)
        for address, size, digest in METHODS:
            original = pe.at_va(address, size)
            if hashlib.sha256(original).hexdigest() != digest or bounded(address, size) != original:
                raise ValueError('loaded getter/poll contract differs')
        table = bounded(0xFC00C4, 124)
        if table != pe.at_va(0xFC00C4, 124) or hashlib.sha256(table).hexdigest() != '6f3f8082e34befa5937ca3ba6855ebcd4804f00302611276480265065609d0c7':
            raise ValueError('loaded complete mouse table differs')

    def snapshot():
        s = base.snapshot(bounded)  # original visible/top/cursor/driver/input checks
        raw = dict(native=s)
        trace.setdefault('snapshots', []).append(raw)  # retained even on refusal
        word = lambda address: struct.unpack('<I', bounded(address, 4))[0]
        hwnd = word(s['mouse']+0x1E8)
        if hwnd != owner['hwnd']:
            raise ValueError('native mouse HWND differs')
        raw['acquired_byte'] = bounded(s['mouse']+7, 1).hex()
        device = base.pointer(word(s['mouse']+0x1A4))
        table = base.pointer(word(device))
        slots = bounded(table, 0x2C)
        raw.update(device=device, device_table=table, device_table_hex=slots.hex(),
                   get_state=struct.unpack_from('<I', slots, 0x24)[0],
                   get_data=struct.unpack_from('<I', slots, 0x28)[0], hwnd=hwnd)
        raw['method'] = module_receipt(bounded, query, raw['get_data'])
        return raw

    contracts()
    first = snapshot()
    guard()
    if identity() != owner:
        raise ValueError('identity changed between snapshots')
    second = snapshot()
    contracts()
    guard()
    if identity() != owner or first != second:
        raise ValueError('unequal snapshots or changed identity')
    if clock()-bounded.start > 2:
        raise ValueError('snapshot deadline exceeded')
    return dict(status='READ_ONLY_EQUAL_PAIR', identity=owner, snapshot=first,
                read_calls=bounded.calls, read_bytes=bounded.bytes, loaded_methods=41,
                atomic_snapshot=False, lifetime_lease=False, input_admitted=False,
                native_frame='SKIPPED', hook_admitted=False)


def validate_expected(e):
    for key in ('pid', 'hwnd', 'created_ticks'):
        if type(e.get(key)) is not int or e[key] <= 0:
            raise ValueError('positive exact identity required: '+key)
    for key in ('exe', 'token', 'session_id'):
        if not isinstance(e.get(key), str) or not e[key].strip():
            raise ValueError('exact identity string required: '+key)
    if type(e.get('deadline')) not in (int, float) or not time.time() < e['deadline'] < float('inf'):
        raise ValueError('current absolute deadline required')


def check_owner(e, owner_name, lease, actual_file, expected_file):
    validate_expected(e)
    if lease.get('owner') != owner_name or lease.get('token') != e['token'] or actual_file != expected_file:
        raise ValueError('current lease or expected file changed')


def check_identity(e, observed):
    if (not observed['thread'] or observed['pid'] != e['pid'] or observed['exit_code'] != 259 or
            observed['created_ticks'] != e['created_ticks'] or
            Path(observed['exe']).resolve() != Path(e['exe']).resolve() or
            observed['title'] != 'The Darkness II' or observed['foreground'] != e['hwnd']):
        raise ValueError('exact PID/creation/path/title/foreground differs')


def live(base, e, owner_name, expected_path, expected_bytes, pe, trace):
    # Existing S21 Win32 read/identity pattern; adds query-only module mapping.
    import ctypes as c
    from ctypes import wintypes as w
    k, u, ps = (c.WinDLL(n, use_last_error=True) for n in ('kernel32', 'user32', 'psapi'))
    def api(lib, name, restype, *types):
        f = getattr(lib, name);f.restype, f.argtypes = restype, types;return f
    op = api(k, 'OpenProcess', w.HANDLE, w.DWORD, w.BOOL, w.DWORD)
    close = api(k, 'CloseHandle', w.BOOL, w.HANDLE)
    rpm = api(k, 'ReadProcessMemory', w.BOOL, w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t))
    image = api(k, 'QueryFullProcessImageNameW', w.BOOL, w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD))
    times = api(k, 'GetProcessTimes', w.BOOL, w.HANDLE, *([c.POINTER(w.FILETIME)]*4))
    exited = api(k, 'GetExitCodeProcess', w.BOOL, w.HANDLE, c.POINTER(w.DWORD))
    window_owner = api(u, 'GetWindowThreadProcessId', w.DWORD, w.HWND, c.POINTER(w.DWORD))
    title = api(u, 'GetWindowTextW', c.c_int, w.HWND, w.LPWSTR, c.c_int)
    foreground = api(u, 'GetForegroundWindow', w.HWND)
    class MBI(c.Structure):
        _fields_ = [('BaseAddress', c.c_void_p), ('AllocationBase', c.c_void_p),
                    ('AllocationProtect', w.DWORD), ('RegionSize', c.c_size_t),
                    ('State', w.DWORD), ('Protect', w.DWORD), ('Type', w.DWORD)]
    vq = api(k, 'VirtualQueryEx', c.c_size_t, w.HANDLE, c.c_void_p, c.POINTER(MBI), c.c_size_t)
    mapped = api(ps, 'GetMappedFileNameW', w.DWORD, w.HANDLE, c.c_void_p, w.LPWSTR, w.DWORD)
    def require(ok):
        if not ok:raise OSError(c.get_last_error(), 'read/query-only Windows API failed')
    def guard():
        check_owner(e, owner_name, json.loads(base.LEASE.read_text()), expected_path.read_bytes(), expected_bytes)
    guard()
    handle = op(0x1410, False, e['pid'])  # QUERY_INFORMATION|QUERY_LIMITED|VM_READ ONLY
    require(handle)
    try:
        def identity():
            pid, code = w.DWORD(), w.DWORD()
            thread = window_owner(e['hwnd'], c.byref(pid))
            name = c.create_unicode_buffer(512);require(title(e['hwnd'], name, len(name)))
            path, size = c.create_unicode_buffer(32768), w.DWORD(32768)
            require(image(handle, 0, path, c.byref(size)))
            stamps = [w.FILETIME() for _ in range(4)];require(times(handle, *(c.byref(f) for f in stamps)))
            require(exited(handle, c.byref(code)))
            created = stamps[0].dwLowDateTime | stamps[0].dwHighDateTime << 32
            check_identity(e, dict(pid=pid.value, thread=thread, exit_code=code.value,
                                  created_ticks=created, exe=path.value, title=name.value, foreground=foreground()))
            return dict(pid=pid.value, created_ticks=created, exe=str(Path(path.value).resolve()),
                        hwnd=e['hwnd'], thread=thread, session_id=e['session_id'])
        def read(address, count):
            data, copied = c.create_string_buffer(count), c.c_size_t()
            ok = rpm(handle, address, data, count, c.byref(copied))
            if not ok or copied.value != count:
                trace['failed_read'] = dict(address=address, requested=count, copied=copied.value, winerror=c.get_last_error())
                raise ValueError('incomplete read; raw address/error retained')
            return data.raw
        def query(address):
            info = MBI();require(vq(handle, address, c.byref(info), c.sizeof(info)) == c.sizeof(info))
            name = c.create_unicode_buffer(32768);n = mapped(handle, info.AllocationBase, name, len(name))
            require(n and n < len(name))
            return dict(allocation_base=info.AllocationBase, region_base=info.BaseAddress,
                        region_size=info.RegionSize, state=info.State, type=info.Type,
                        protect=info.Protect, mapped_path=name.value)
        return collect(base, read, pe, identity, guard, query, trace)
    finally:
        close(handle)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--live', action='store_true')
    p.add_argument('--expected', type=Path)
    p.add_argument('--owner-name')
    p.add_argument('--output', type=Path)
    a = p.parse_args(argv)
    if not a.live:
        print('INERT: no native API/process/file reads');return 0
    if not a.expected or not a.owner_name or not a.owner_name.strip() or not a.output:
        p.error('--live requires --expected, --owner-name, --output')
    trace = {}
    with a.output.open('x', encoding='utf-8') as out:
        try:
            raw = a.expected.read_bytes();e = json.loads(raw);validate_expected(e)
            base = load_inspector()
            blob = Path(e['exe']).read_bytes()
            if hashlib.sha256(blob).hexdigest() != base.SHA256:
                raise ValueError('not pinned offline EXE')
            pe = base.PE(blob)
            result = live(base, e, a.owner_name, a.expected, raw, pe, trace)
        except (OSError, ValueError, KeyError, struct.error) as error:
            result = dict(status='REFUSED', error=str(error), input_admitted=False)
        result.update(raw=trace, helper_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                      inspector_sha256=INSPECTOR_SHA, captured_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
        json.dump(result, out, indent=2)
    print(result['status'])
    return 0 if result['status'] == 'READ_ONLY_EQUAL_PAIR' else 2


if __name__ == '__main__':
    raise SystemExit(main())
