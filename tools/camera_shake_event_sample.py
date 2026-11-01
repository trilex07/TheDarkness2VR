"""Read-only multi-owner event capture; does not install or change the mod.

Run while the user walks, turns, encounters the train and takes damage. Samples
are asynchronous observations, not proof of native update/render ordering.
"""
import argparse
import ctypes as ct
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time

from camera_shake_owner_scan import EXE_SHA


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int)
    parser.add_argument('--seconds', type=float, default=120)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('seconds must be between 1 and 300')
    pid = args.pid
    deadline = time.monotonic() + 180
    while pid is None:
        rows = subprocess.check_output(['tasklist', '/FI', 'IMAGENAME eq DarknessII.exe',
                                        '/FO', 'CSV', '/NH'], text=True)
        match = re.search(r'"DarknessII\.exe","(\d+)"', rows, re.I)
        if match:
            pid = int(match[1])
            break
        if time.monotonic() >= deadline:
            raise RuntimeError('No game process within 180 seconds')
        time.sleep(1)
    api = ct.WinDLL('kernel32', use_last_error=True)
    api.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
    api.OpenProcess.restype = ct.c_void_p
    api.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p,
                                     ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.CloseHandle.argtypes = [ct.c_void_p]
    api.QueryFullProcessImageNameW.argtypes = [ct.c_void_p, ct.c_ulong,
                                             ct.c_wchar_p, ct.POINTER(ct.c_ulong)]
    process = api.OpenProcess(0x1010, False, pid)  # QUERY_LIMITED_INFORMATION | VM_READ
    if not process:
        raise ct.WinError(ct.get_last_error())
    try:
        name, length = ct.create_unicode_buffer(32768), ct.c_ulong(32768)
        if not api.QueryFullProcessImageNameW(process, 0, name, ct.byref(length)):
            raise ct.WinError(ct.get_last_error())
        exe = Path(name.value)
        if exe.name.lower() != 'darknessii.exe' or hashlib.sha256(exe.read_bytes()).hexdigest().upper() != EXE_SHA:
            raise RuntimeError('Refusing unpinned game executable')
        def read(address, size):
            if address < 0x10000 or address + size >= 0x80000000:
                return None
            data, got = ct.create_string_buffer(size), ct.c_size_t()
            if not api.ReadProcessMemory(process, address, data, size, ct.byref(got)) or got.value != size:
                return None
            return data.raw
        def word(address):
            data = read(address, 4)
            return struct.unpack('<I', data)[0] if data else 0
        def camera_type(camera):
            table = word(camera)
            # protected_left_aim.inl prepares 71 entries and replaces only 39.
            # Match every other entry against the pinned original table.
            actual, stock = read(table, 71 * 4), read(0xFFC7C0, 71 * 4)
            return bool(actual and stock and actual[:39*4] == stock[:39*4] and
                        actual[40*4:] == stock[40*4:] and word(table + 39*4))
        print(f'GAME FOUND pid={pid}; waiting for gameplay camera', flush=True)
        deadline = time.monotonic() + 180
        while True:
            scan = subprocess.check_output([sys.executable, str(Path(__file__).with_name(
                'camera_shake_owner_scan.py')), str(pid)], text=True)
            owners = [int(x, 16) for x in re.search(r'shake_owners=(.*)', scan)[1].split(',') if x]
            cameras = [int(x, 16) for x in re.findall(r'camera=([0-9A-F]{8}).*selected_candidate=True', scan)]
            avatars = [int(text, 16) for text in re.findall(r'^avatar=([0-9A-F]{8})', scan, re.M)]
            for avatar in avatars:
                camera = word(word(avatar + 0x62c))
                if (word(avatar) == 0x1001630 and camera_type(camera) and
                        word(word(camera + 0x30)) == avatar and camera not in cameras):
                    cameras.append(camera)
            if owners and cameras:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError('No gameplay camera/owners within 180 seconds')
            time.sleep(5)
        def floats(data, offset, count):
            return list(struct.unpack_from('<' + 'f' * count, data, offset))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('x', encoding='utf-8') as out:
            out.write(json.dumps({'kind': 'header', 'pid': pid, 'exe_sha256': EXE_SHA,
                'dll_sha256': hashlib.sha256(exe.with_name('d3d9.dll').read_bytes()).hexdigest().upper(),
                'scan': scan, 'warning': 'candidate objects; asynchronous read-only samples'}) + '\n')
            print(f'CAPTURE BEGIN pid={pid} owners={len(owners)} seconds={args.seconds} output={args.output}', flush=True)
            start = time.perf_counter()
            samples, next_report = 0, 0
            while time.perf_counter() - start < args.seconds:
                elapsed = time.perf_counter() - start
                row = {'t': round(elapsed, 4), 'owners': [], 'cameras': []}
                for avatar in avatars:
                    if word(avatar) != 0x1001630:
                        continue
                    camera = word(word(avatar + 0x62c))
                    if camera_type(camera) and word(word(camera + 0x30)) == avatar and camera not in cameras:
                        cameras.append(camera)
                for owner in owners:
                    data = read(owner, 0x1310)
                    if not data or struct.unpack_from('<I', data)[0] != 0xFDF170:
                        continue
                    blocks = [{'offset': hex(b), 'position': floats(data, b, 3),
                               'rotation': floats(data, b + 0x38, 3),
                               'rot_factors': floats(data, b + 0x10, 3),
                               'pos_factor': floats(data, b + 0x24, 1)[0],
                               'impulse': floats(data, b + 0x20, 1)[0],
                               'ambient_movement': floats(data, b + 0x2c, 2)}
                              for b in (0x8C0, 0x910, 0x960, 0xCA0)]
                    row['owners'].append({'address': hex(owner), 'blocks': blocks,
                        'impact_enabled': [data[0x1170 + k + 0x10] for k in (0, 0x70, 0xe0, 0x130)],
                        'impact_position': floats(data, 0x12f0, 3),
                        'impact_rotation': floats(data, 0x1300, 3),
                        'sum_position': floats(data, 0x1f0, 3),
                        'sum_rotation': floats(data, 0x20c, 3)})
                for camera in cameras:
                    if not camera_type(camera):
                        continue
                    avatar = word(word(camera + 0x30))
                    if word(avatar) != 0x1001630 or word(word(avatar + 0x62c)) != camera:
                        continue
                    data = read(camera + 0x80, 64)
                    parent = word(word(avatar + 0x1f8c))
                    root = read(parent + 0x140, 64) if word(parent) == 0xFFC300 else None
                    row['cameras'].append({'address': hex(camera), 'avatar': hex(avatar),
                        'matrix': floats(data, 0, 16) if data else None,
                        'parent_matrix': floats(root, 0, 16) if root else None})
                out.write(json.dumps(row) + '\n')
                samples += 1
                if elapsed >= next_report:
                    out.flush()
                    print(f'CAPTURE {elapsed:.0f}s samples={samples} liveOwners={len(row["owners"])} cameras={len(row["cameras"])}', flush=True)
                    next_report += 15
                if not row['owners']:
                    print('All candidate owners disappeared; stopping without following stale pointers', flush=True)
                    break
                time.sleep(.01)
            print(f'CAPTURE END samples={samples}', flush=True)
    finally:
        api.CloseHandle(process)


if __name__ == '__main__':
    main()
