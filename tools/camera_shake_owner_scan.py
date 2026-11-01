"""One-shot read-only search for the selected avatar and shake settings owner.

Exact stock DarknessII.exe only. No debugger, process write or pointer patch.
This reports candidate object links; a live ownership/lifetime witness is still
needed before a heap intervention.
"""
import ctypes as ct
import hashlib
import struct
import sys
from pathlib import Path

EXE_SHA = "D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1"
AVATAR_TABLE = 0x01001630
SHAKE_TABLE = 0x00FDF170
CAMERA_TABLE = 0x00FFC7C0
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}

class MBI(ct.Structure):
    _fields_ = [("BaseAddress", ct.c_void_p), ("AllocationBase", ct.c_void_p),
                ("AllocationProtect", ct.c_ulong), ("PartitionId", ct.c_ushort),
                ("RegionSize", ct.c_size_t), ("State", ct.c_ulong),
                ("Protect", ct.c_ulong), ("Type", ct.c_ulong)]

def main(pid: int) -> None:
    api = ct.WinDLL("kernel32", use_last_error=True)
    api.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
    api.OpenProcess.restype = ct.c_void_p
    api.VirtualQueryEx.argtypes = [ct.c_void_p, ct.c_void_p, ct.POINTER(MBI), ct.c_size_t]
    api.VirtualQueryEx.restype = ct.c_size_t
    api.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.ReadProcessMemory.restype = ct.c_int
    api.QueryFullProcessImageNameW.argtypes = [ct.c_void_p, ct.c_ulong, ct.c_wchar_p, ct.POINTER(ct.c_ulong)]
    api.QueryFullProcessImageNameW.restype = ct.c_int
    api.CloseHandle.argtypes = [ct.c_void_p]
    process = api.OpenProcess(0x0010 | 0x0400, 0, pid)
    if not process:
        raise OSError(ct.get_last_error(), "OpenProcess read-only failed")
    try:
        name = ct.create_unicode_buffer(32768)
        length = ct.c_ulong(len(name))
        if not api.QueryFullProcessImageNameW(process, 0, name, ct.byref(length)):
            raise OSError(ct.get_last_error(), "Cannot verify game image")
        path = Path(name.value)
        digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
        if path.name.lower() != "darknessii.exe" or digest != EXE_SHA:
            raise RuntimeError(f"Refusing unpinned executable SHA256={digest}")
        def read(address: int, size: int) -> bytes | None:
            if not address or address + size >= 0x80000000:
                return None
            buffer = ct.create_string_buffer(size)
            got = ct.c_size_t()
            if not api.ReadProcessMemory(process, ct.c_void_p(address), buffer, size, ct.byref(got)) or got.value != size:
                return None
            return buffer.raw
        def word(address: int) -> int | None:
            data = read(address, 4)
            return struct.unpack("<I", data)[0] if data else None
        found = {AVATAR_TABLE: [], SHAKE_TABLE: [], CAMERA_TABLE: []}
        patterns = {v: struct.pack("<I", v) for v in found}
        address = 0x10000
        searched = 0
        while address < 0x80000000:
            region = MBI()
            if not api.VirtualQueryEx(process, ct.c_void_p(address), ct.byref(region), ct.sizeof(region)):
                address += 0x1000
                continue
            start = region.BaseAddress or address
            end = start + region.RegionSize
            if end <= address:
                break
            if (region.State == 0x1000 and region.Type == 0x20000 and
                    region.Protect & 0x101 == 0 and region.Protect & 0xFF in READABLE):
                pos = start
                while pos < end:
                    size = min(1 << 20, end - pos)
                    data = read(pos, size)
                    if data:
                        searched += size
                        for table, pattern in patterns.items():
                            offset = data.find(pattern)
                            while offset >= 0:
                                obj = pos + offset
                                if obj & 3 == 0 and len(found[table]) < 128:
                                    found[table].append(obj)
                                offset = data.find(pattern, offset + 4)
                    pos += size
            address = end
        avatars, owners, cameras = found[AVATAR_TABLE], found[SHAKE_TABLE], found[CAMERA_TABLE]
        print(f"exe_sha256={digest} scanned_private_bytes={searched} avatar_candidates={len(avatars)} shake_candidates={len(owners)} camera_candidates={len(cameras)}")
        owner_set = set(owners)
        for avatar in avatars[:16]:
            data = read(avatar, 0x2000)
            if not data:
                print(f"avatar={avatar:08X} unreadable")
                continue
            links = []
            for offset in range(4, len(data), 4):
                pointer = struct.unpack_from("<I", data, offset)[0]
                if pointer in owner_set:
                    links.append((offset, pointer, "direct"))
                elif pointer and pointer < 0x80000000 and word(pointer) in owner_set:
                    links.append((offset, word(pointer), "one-indirection"))
            shake_ref = word(avatar + 0x5E0)
            shake = word(shake_ref) if shake_ref else None
            values = []
            if shake in owner_set:
                for offset in (0x8C0,0x910,0x960,0xCA0):
                    raw = read(shake + offset, 0x44)
                    if raw:
                        output = struct.unpack_from("<3f", raw, 0)
                        strengths = struct.unpack_from("<3f", raw, 0x20)
                        strengths += struct.unpack_from("<2f", raw, 0x2C)
                        values.append((hex(offset), tuple(round(x,4) for x in output),
                                       tuple(round(x,4) for x in strengths)))
            print(f"avatar={avatar:08X} ref5E0={shake_ref or 0:08X} shake={shake or 0:08X} links=" + str([(hex(o), hex(p), k) for o,p,k in links[:24]]))
            if values:
                print(f"  noise_fields={values}")
        for camera in cameras[:16]:
            avatar_ref = word(camera + 0x30)
            avatar = word(avatar_ref) if avatar_ref else None
            print(f"camera={camera:08X} ref30={avatar_ref or 0:08X} avatar={avatar or 0:08X} selected_candidate={avatar in avatars}")
        print("shake_owners=" + ",".join(f"{x:08X}" for x in owners[:32]))
    finally:
        api.CloseHandle(process)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: camera_shake_owner_scan.py PID")
    main(int(sys.argv[1]))
