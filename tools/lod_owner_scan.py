"""Read-only, one-shot scan for the SHA-pinned native scene owner vtable.

Run only while DarknessII.exe is in a scene. The scan never writes process
memory, changes debug registers, or attaches a debugger. Candidate pointers
still need native ownership and lifetime validation before any LOD change.
"""

import argparse
import ctypes as ct
import struct


PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
MEM_COMMIT = 0x1000
MEM_PRIVATE = 0x20000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}
OWNER_VTABLE = 0x00FF8818  # stock EXE 56CD8B and FF88AC -> 9922F0
PATTERN = struct.pack("<I", OWNER_VTABLE)


class MEMORY_BASIC_INFORMATION(ct.Structure):
    _fields_ = [
        ("BaseAddress", ct.c_void_p),
        ("AllocationBase", ct.c_void_p),
        ("AllocationProtect", ct.c_ulong),
        ("PartitionId", ct.c_ushort),
        ("RegionSize", ct.c_size_t),
        ("State", ct.c_ulong),
        ("Protect", ct.c_ulong),
        ("Type", ct.c_ulong),
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    args = parser.parse_args()
    api = ct.WinDLL("kernel32", use_last_error=True)
    api.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
    api.OpenProcess.restype = ct.c_void_p
    api.VirtualQueryEx.argtypes = [ct.c_void_p, ct.c_void_p, ct.POINTER(MEMORY_BASIC_INFORMATION), ct.c_size_t]
    api.VirtualQueryEx.restype = ct.c_size_t
    api.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.ReadProcessMemory.restype = ct.c_int
    api.CloseHandle.argtypes = [ct.c_void_p]
    handle = api.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, 0, args.pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess read-only failed")

    def read(address: int, size: int) -> bytes | None:
        if not address or address + size >= 0x80000000:
            return None
        buffer = ct.create_string_buffer(size)
        got = ct.c_size_t()
        if not api.ReadProcessMemory(handle, ct.c_void_p(address), buffer, size, ct.byref(got)) or got.value != size:
            return None
        return buffer.raw

    def word(address: int) -> int | None:
        data = read(address, 4)
        return struct.unpack("<I", data)[0] if data else None

    try:
        address = 0x10000
        searched = 0
        candidates: list[tuple[int, int, int, int | None, int | None]] = []
        while address < 0x80000000:
            region = MEMORY_BASIC_INFORMATION()
            if not api.VirtualQueryEx(handle, ct.c_void_p(address), ct.byref(region), ct.sizeof(region)):
                address += 0x1000
                continue
            start = region.BaseAddress or address
            size = region.RegionSize
            next_address = start + size
            if next_address <= address:
                break
            if (region.State == MEM_COMMIT and region.Type == MEM_PRIVATE and
                    region.Protect & (PAGE_GUARD | PAGE_NOACCESS) == 0 and
                    region.Protect & 0xFF in READABLE):
                offset = start
                while offset < next_address:
                    length = min(1 << 20, next_address - offset)
                    data = read(offset, length)
                    if data:
                        searched += len(data)
                        found = data.find(PATTERN)
                        while found >= 0:
                            owner = offset + found
                            if owner % 4 == 0:
                                renderer = word(owner + 0x274)
                                if renderer:
                                    renderer_table = word(renderer)
                                    owner_scene = word(renderer + 0x594)
                                    candidates.append((owner, renderer, renderer_table or 0,
                                                       owner_scene, word(owner + 0x2A4)))
                            found = data.find(PATTERN, found + 4)
                    offset += length
            address = next_address
        print(f"searched_private_bytes={searched} candidates={len(candidates)}")
        for owner, renderer, table, scene, camera_ref in candidates[:128]:
            scene_value = word(scene) if scene else None
            list_ref = word(renderer + 0x5B4)
            list_value = word(list_ref) if list_ref else None
            list_count = word(list_value + 0x200) if list_value else None
            print(f"owner={owner:08X} renderer={renderer:08X} table={table:08X} "
                  f"renderer+594={scene or 0:08X} -> {scene_value or 0:08X} "
                  f"renderer+5B4={list_ref or 0:08X} -> {list_value or 0:08X} "
                  f"list+200={list_count or 0:08X} "
                  f"owner+2A4={camera_ref or 0:08X}")
    finally:
        api.CloseHandle(handle)


if __name__ == "__main__":
    main()
