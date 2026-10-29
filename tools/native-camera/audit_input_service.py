"""Read-only receipt for the pinned native client/input initialization ABI.

Default: static file only. Optional --pid makes one read-only HWND/input-chain
observation, never starts a game, calls a factory, hooks or injects input.
Neither mode establishes menu state, a rendered menu plane or a lifetime lease.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from audit_camera import PE

SHA256 = "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
# Independently recovered complete instruction ranges used by the HWND route.
LIVE_RANGES = ((0x40DEC0, 0xB9), (0xD993C0, 0x124), (0xC2C1A0, 0xA7B),
               (0x9F4580, 0x86B), (0x7A8570, 0x69), (0xAAE390, 0xA3))


def observe_chain(read_word, hwnd, framework):
    """One snapshot; caller must recheck HWND identity and chain afterwards."""
    def pointer(value):
        if type(value) is not int or value < 0x10000 or value > 0xFFFFFFFC or value % 4:
            raise ValueError("null, unaligned or out-of-range x86 object/reference")
        return value

    def field(base, offset):
        pointer(base)
        if base > 0xFFFFFFFC - offset:
            raise ValueError("x86 field address overflow")
        return read_word(base + offset)

    framework = pointer(framework)
    if field(framework, 0) != 0xFC8930 or field(framework, 0xF8) != hwnd:
        raise ValueError("window userdata is not the matching native framework")
    client_ref = pointer(field(framework, 0xA8))
    client = pointer(field(client_ref, 0))
    input_ref = pointer(field(client, 0x374))
    input_object = pointer(field(input_ref, 0))
    if field(input_object, 0) != 0xFBFCD0:
        raise ValueError("input object is not the pinned Windows manager type")
    return dict(framework=framework, client_ref=client_ref, client=client,
                input_ref=input_ref, input_object=input_object)


def observe_live(pid, exe_blob, exe_path):
    import ctypes as c
    import os
    from ctypes import wintypes as w
    if os.name != "nt" or pid <= 0:
        raise ValueError("--pid requires Windows and a positive existing process ID")
    kernel = c.WinDLL("kernel32", use_last_error=True)
    user = c.WinDLL("user32", use_last_error=True)
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.CloseHandle.restype = w.BOOL
    kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    kernel.ReadProcessMemory.restype = w.BOOL
    kernel.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    kernel.QueryFullProcessImageNameW.restype = w.BOOL
    user.GetWindowLongW.argtypes = [w.HWND, c.c_int]
    user.GetWindowLongW.restype = w.LONG  # x86 target's LONG userdata, normalized below
    user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
    user.GetWindowThreadProcessId.restype = w.DWORD
    callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, c.c_ssize_t)
    user.EnumWindows.argtypes = [callback_type, c.c_ssize_t]
    user.EnumWindows.restype = w.BOOL
    handle = kernel.OpenProcess(0x1000 | 0x10, False, pid)  # query limited + VM_READ only
    if not handle:
        raise OSError(c.get_last_error(), "read-only OpenProcess failed")
    try:
        path = c.create_unicode_buffer(32768)
        length = w.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, c.byref(length)):
            raise OSError(c.get_last_error(), "cannot identify target executable")
        if Path(path.value).resolve() != exe_path.resolve():
            raise ValueError("PID executable differs from the audited file")

        def read(address, count):
            data = c.create_string_buffer(count)
            copied = c.c_size_t()
            if not kernel.ReadProcessMemory(handle, address, data, count, c.byref(copied)) or copied.value != count:
                raise ValueError("target snapshot unavailable or incomplete")
            return data.raw

        pe = PE(exe_blob)
        for address, count in LIVE_RANGES:
            if read(address, count) != pe.at_va(address, count):
                raise ValueError("loaded native contract differs from the pinned file")

        def word(address):
            return struct.unpack("<I", read(address, 4))[0]

        def identity(hwnd):
            owner = w.DWORD()
            thread = user.GetWindowThreadProcessId(hwnd, c.byref(owner))
            return thread, owner.value, user.GetWindowLongW(hwnd, -21) & 0xFFFFFFFF

        windows = []
        @callback_type
        def collect(hwnd, _):
            owner = w.DWORD()
            if user.GetWindowThreadProcessId(hwnd, c.byref(owner)) and owner.value == pid:
                windows.append(hwnd)
            return True
        if not user.EnumWindows(collect, 0):
            raise OSError(c.get_last_error(), "window enumeration failed")
        receipts, rejected = [], []
        for hwnd in windows:
            try:
                before = identity(hwnd)
                if not before[0] or before[1] != pid:
                    raise ValueError("window ownership changed")
                first = observe_chain(word, hwnd, before[2])
                second = observe_chain(word, hwnd, before[2])
                if first != second or identity(hwnd) != before:
                    raise ValueError("window or native chain changed during observation")
                receipts.append(dict(hwnd=hex(hwnd), window_thread=before[0],
                                     **{name: hex(value) for name, value in first.items()}))
            except ValueError as error:
                rejected.append(dict(hwnd=hex(hwnd), reason=str(error)))
        return dict(status="READ_ONLY_SNAPSHOT" if receipts else "NO_MATCHING_SNAPSHOT",
                    pid=pid, windows=receipts, rejected=rejected,
                    lifetime_lease=False, menu_visibility_verified=False,
                    menu_surface_verified=False, input_injection_admitted=False)
    finally:
        kernel.CloseHandle(handle)


def inspect(blob):
    digest = hashlib.sha256(blob).hexdigest()
    pe = PE(blob)
    if digest != SHA256 or pe.machine != 0x14C or pe.base != 0x400000:
        raise ValueError("unrecognized native image; no service contract admitted")
    # The static initializer selects this exact global and constructor. The
    # constructor writes FBAC40; slot +1C is a no-stack-arguments field getter.
    initializer = pe.at_va(0xEA4FD0, 10)
    if initializer[:6] != bytes.fromhex("b9f8e30d01e8"):
        raise ValueError("service-global initializer changed")
    target = 0xEA4FDA + struct.unpack("<i", initializer[6:])[0]
    if target != 0xC5CC40:
        raise ValueError("service constructor changed")
    if pe.at_va(target, 9) != bytes.fromhex("8bc157c70040acfb00"):
        raise ValueError("service vtable assignment changed")
    getter = struct.unpack("<I", pe.at_va(0xFBAC40 + 0x1C, 4))[0]
    if getter != 0x74B5F0 or pe.at_va(getter, 4) != bytes.fromhex("8d4150c3"):
        raise ValueError("service getter is not the verified no-pop getter")
    # Initializer C2C1A0 reserves 2C and saves four registers: entry ESP-3C.
    # Its reads at +48/+4C/+50 therefore name arguments 3/4/5, respectively.
    # Seven pushed words survive the getter; RET 1C consumes all seven.
    if pe.at_va(0xC2C1A0, 7) != bytes.fromhex("83ec2c53555657"):
        raise ValueError("client initializer frame changed")
    if pe.at_va(0xC2CC18, 3) != bytes.fromhex("c21c00"):
        raise ValueError("client initializer argument cleanup changed")
    # Concrete MouseDeviceWin API recovered from its pinned native table.
    # These checks describe existing methods; they do not grant object lifetime
    # or call/injection permission to a later reader of this static receipt.
    mouse_methods = {0x5C: 0x8A1470, 0x6C: 0x603190, 0x70: 0xC4B690}
    for slot, target in mouse_methods.items():
        if struct.unpack("<I", pe.at_va(0xFC00C8 + slot, 4))[0] != target:
            raise ValueError("native mouse method changed")
    if pe.at_va(0x603190, 27) != bytes.fromhex("8b811c0200008b54240489028b81200200008b4c24088901c20800"):
        raise ValueError("native mouse position getter changed")
    return {
        "status": "STATIC_CONTRACT_PASS", "exe_sha256": digest,
        "native_mouse_cursor": {
            "vtable": "0xFC00C8",
            "get_position": "slot +0x6C, 0x603190; void thiscall(this, int* x, int* y)",
            "set_position": "slot +0x70, 0xC4B690; void thiscall(this, int x, int y)",
            "position_fields": ["0x21C", "0x220"],
            "coordinates": "native HWND client pixels; not render-target pixels",
            "event_decoder": "slot +0x5C, 0x8A1470; consumes one native 20-byte event",
            "event_return": "entry appData equals 1; NOT button-down state",
            "live_owner_thread_and_lifetime_verified": False,
        },
        "framework_client_reference_offset": "0xA8",
        "reference_assignment": "0x7A8570; pointer to intrusive reference, not object",
        "client_initializer": "0xC2C1A0; this=client, seven stack arguments",
        "arguments": ["framework", "service-global+0x50", "graphics reference address",
                      "sound reference address", "input reference address",
                      "framework+0x10 reference value", "0x1102080"],
        "client_reference_fields": {"graphics": "0x2D0", "sound": "0x2C4", "input": "0x374"},
        "input_object_path_from_valid_framework": [
            "client_ref = load_ptr(framework+0xA8)", "client = load_ptr(client_ref)",
            "input_ref = load_ptr(client+0x374)", "input_object = load_ptr(input_ref)"],
        "live_framework_address_verified": False, "live_input_type_verified": False,
        "menu_visibility_verified": False, "menu_surface_verified": False,
        "input_injection_admitted": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--pid", type=int, help="optional single read-only observation of an already running game")
    args = parser.parse_args()
    try:
        blob = args.exe.read_bytes()
        result = inspect(blob)
        if args.pid is not None:
            result["observation"] = observe_live(args.pid, blob, args.exe)
    except (OSError, ValueError, struct.error) as error:
        print(json.dumps({"status": "BLOCKED", "error": str(error)}))
        return 2
    print(json.dumps(result, indent=2))
    if args.pid is not None and not result["observation"]["windows"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
