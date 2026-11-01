"""Read-only, one-shot scan for the SHA-pinned native scene owner vtable.

Run only while DarknessII.exe is in a scene. The scan never writes process
memory, changes debug registers, or attaches a debugger. Candidate pointers
still need native ownership and lifetime validation before any LOD change.
"""

import argparse
import ctypes as ct
import hashlib
from pathlib import Path
import struct


PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
MEM_COMMIT = 0x1000
MEM_PRIVATE = 0x20000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}
OWNER_VTABLE = 0x00FF8818  # stock EXE 56CD8B and FF88AC -> 9922F0
RENDERER_VTABLE = 0x00FF89E8  # prior live owner+274 renderer; stock 77751D path
STOCK_EXE_SHA256 = "D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1"
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
    api.QueryFullProcessImageNameW.argtypes = [ct.c_void_p, ct.c_ulong, ct.c_wchar_p, ct.POINTER(ct.c_ulong)]
    api.QueryFullProcessImageNameW.restype = ct.c_int
    handle = api.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, 0, args.pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess read-only failed")

    image_path = ct.create_unicode_buffer(32768)
    image_length = ct.c_ulong(len(image_path))
    if not api.QueryFullProcessImageNameW(handle, 0, image_path, ct.byref(image_length)):
        api.CloseHandle(handle)
        raise OSError(ct.get_last_error(), "Cannot verify process image")
    image = Path(image_path.value)
    digest = hashlib.sha256(image.read_bytes()).hexdigest().upper()
    if image.name.lower() != "darknessii.exe" or digest != STOCK_EXE_SHA256:
        api.CloseHandle(handle)
        raise RuntimeError(f"Refusing different executable: {image} SHA256={digest}")

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

    def view_list(renderer: int) -> tuple[str, list[tuple[int, int, int | None, int | None, int | None]]]:
        # Stock 7774C9..77751D: iterate renderer+124 list; the view is the
        # inline subobject at node+10, wrapped on the stack for
        # AEA1A0 -> 5828B0 -> 8513A0. Do not dereference node+10.
        if word(renderer) != RENDERER_VTABLE:
            return "renderer-type-mismatch", []
        sentinel = renderer + 0x124
        node = word(sentinel)
        if node is None:
            return "unreadable-head", []
        views: list[tuple[int, int, int | None, int | None, int | None]] = []
        seen: set[int] = set()
        while node != sentinel:
            if len(views) >= 64 or node < 0x10000 or node & 3 or node in seen:
                return "truncated-or-invalid-list", views
            seen.add(node)
            next_node = word(node)
            view = node + 0x10
            if next_node is None or view < 0x10000 or view & 3:
                return "unreadable-node", views
            kind, enabled, mode = word(view + 0x178), word(view + 0x618), word(view + 0x61C)
            if kind is None or enabled is None or mode is None:
                return "unreadable-view", views
            views.append((node, view, kind, enabled, mode))
            node = next_node
        if word(sentinel) != (views[0][0] if views else sentinel):
            return "head-changed-during-scan", views
        return "complete", views

    try:
        # Stock 8513A3..851462: global LOD gate, force-last gate and
        # signed selector override. View mode is used only when override < 0.
        lod_enable = word(0x0110DE54)
        force_last = word(0x0110DF40)
        mode_override = word(0x0110DFF0)
        signed_override = ct.c_int32(mode_override).value if mode_override is not None else None
        print(f"lod_global_enable={lod_enable & 0xFF if lod_enable is not None else 'unreadable'} "
              f"force_last={force_last & 0xFF if force_last is not None else 'unreadable'} "
              f"mode_override={signed_override if signed_override is not None else 'unreadable'}")
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
        print(f"exe_sha256={digest} searched_private_bytes={searched} candidates={len(candidates)}")
        for owner, renderer, table, scene, camera_ref in candidates[:128]:
            scene_value = word(scene) if scene else None
            list_ref = word(renderer + 0x5B4)
            list_value = word(list_ref) if list_ref else None
            object_list_ref = word(list_value + 0x200) if list_value else None
            print(f"owner={owner:08X} renderer={renderer:08X} table={table:08X} "
                  f"renderer+594={scene or 0:08X} -> {scene_value or 0:08X} "
                  f"renderer+5B4={list_ref or 0:08X} -> {list_value or 0:08X} "
                  f"object_list_ref+200={object_list_ref or 0:08X} "
                  f"owner+2A4={camera_ref or 0:08X}")
            status, views = view_list(renderer)
            print(f"  view_list={status} sampled={len(views)} (read-only; not a lifetime lease)")
            for node, view, kind, enabled, mode in views[:16]:
                selected_mode = signed_override if signed_override is not None and signed_override >= 0 else mode
                selector = ("unknown" if None in (lod_enable, force_last, signed_override, enabled, mode) else
                            "disabled/005F6160" if not lod_enable or not enabled else
                            "force-last/006281F0" if force_last else
                            "projection/006BF3D0" if selected_mode and selected_mode > 0 else
                            "distance/00AE30C0")
                print(f"  node={node:08X} view={view:08X} kind={kind if kind is not None else 'unreadable'} "
                      f"enable618={enabled & 0xFF if enabled is not None else 'unreadable'} "
                      f"mode61C={mode if mode is not None else 'unreadable'} "
                      f"predicted_selector={selector} (no call witness)")
    finally:
        api.CloseHandle(handle)


if __name__ == "__main__":
    main()
