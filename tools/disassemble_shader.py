"""Disassemble local D3D bytecode with Windows D3DCompiler, without a device."""
import argparse
import ctypes as C
from pathlib import Path


def disassemble(path):
    data = path.read_bytes()
    compiler = C.WinDLL("d3dcompiler_47.dll")
    fn = compiler.D3DDisassemble
    fn.argtypes = [C.c_void_p, C.c_size_t, C.c_uint, C.c_char_p,
                   C.POINTER(C.c_void_p)]
    fn.restype = C.c_long
    blob = C.c_void_p()
    buffer = C.create_string_buffer(data)
    hr = fn(buffer, len(data), 0, None, C.byref(blob))
    if hr < 0:
        raise RuntimeError(f"D3DDisassemble failed: {hr & 0xffffffff:08x}")
    table = C.cast(blob, C.POINTER(C.POINTER(C.c_void_p))).contents
    release = C.WINFUNCTYPE(C.c_ulong, C.c_void_p)(table[2])
    try:
        pointer = C.WINFUNCTYPE(C.c_void_p, C.c_void_p)(table[3])(blob)
        size = C.WINFUNCTYPE(C.c_size_t, C.c_void_p)(table[4])(blob)
        return C.string_at(pointer, size).rstrip(b"\0").decode("utf-8")
    finally:
        release(blob)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shader", type=Path, nargs="+")
    args = parser.parse_args()
    for shader in args.shader:
        output = shader.with_suffix(".asm.txt")
        output.write_text(disassemble(shader), encoding="utf-8")
        print(output)
