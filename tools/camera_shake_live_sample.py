"""Read-only 10-second sample of the verified live avatar->shake owner chain."""
import ctypes as ct
import hashlib
import math
from pathlib import Path
import struct
import sys
import time

EXE_SHA = "D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1"
AVATAR_TABLE = 0x01001630
SHAKE_TABLE = 0x00FDF170
OFFSETS = (0x8C0, 0x910, 0x960, 0xCA0)

def main(pid: int, avatar: int) -> None:
    api = ct.WinDLL("kernel32", use_last_error=True)
    api.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
    api.OpenProcess.restype = ct.c_void_p
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
        def read(address: int, size: int) -> bytes:
            data = ct.create_string_buffer(size)
            got = ct.c_size_t()
            if not api.ReadProcessMemory(process, ct.c_void_p(address), data, size, ct.byref(got)) or got.value != size:
                raise RuntimeError(f"Unstable/unreadable owner address {address:08X}")
            return data.raw
        def word(address: int) -> int:
            return struct.unpack("<I", read(address, 4))[0]
        def owner() -> int:
            if word(avatar) != AVATAR_TABLE:
                raise RuntimeError("Selected avatar type changed")
            ref = word(avatar + 0x5E0)
            current = word(ref)
            if word(current) != SHAKE_TABLE:
                raise RuntimeError("Shake owner type changed")
            return current
        initial = owner()
        counts = [0] * len(OFFSETS)
        maxima = [[0.0] * 3 for _ in OFFSETS]
        peaks = [None] * len(OFFSETS)
        started = time.perf_counter()
        samples = 0
        while time.perf_counter() - started < 10.0:
            if owner() != initial:
                raise RuntimeError("Owner changed during sample")
            elapsed = time.perf_counter() - started
            for i, offset in enumerate(OFFSETS):
                raw = read(initial + offset, 0x44)
                output = struct.unpack_from("<3f", raw)
                strength = (struct.unpack_from("<f", raw, 0x20)[0],
                            struct.unpack_from("<f", raw, 0x2C)[0],
                            struct.unpack_from("<f", raw, 0x30)[0])
                if not all(math.isfinite(x) for x in output + strength):
                    raise RuntimeError("Non-finite shake field")
                magnitude = math.sqrt(sum(x * x for x in output))
                if magnitude > 0.0001 or any(abs(x) > 0.0001 for x in strength):
                    counts[i] += 1
                if peaks[i] is None or magnitude > peaks[i][0]:
                    peaks[i] = (magnitude, elapsed, output, strength)
                for j in range(3):
                    maxima[i][j] = max(maxima[i][j], abs(strength[j]))
            samples += 1
            time.sleep(0.02)
        print(f"exe_sha256={digest} avatar={avatar:08X} owner={initial:08X} samples={samples}")
        for i, offset in enumerate(OFFSETS):
            print(f"slot={offset:03X} active_samples={counts[i]} max_strength={maxima[i]} peak={peaks[i]}")
    finally:
        api.CloseHandle(process)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: camera_shake_live_sample.py PID AVATAR_HEX")
    main(int(sys.argv[1]), int(sys.argv[2], 16))
