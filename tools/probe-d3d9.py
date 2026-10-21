"""Read-only live COM/vtable and function-byte inspection; addresses from current log."""
import argparse, ctypes as C, ctypes.wintypes as W, struct
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
ap=argparse.ArgumentParser(); ap.add_argument('pid',type=int); ap.add_argument('--object',action='append',default=[]); ap.add_argument('--code',action='append',default=[]); a=ap.parse_args()
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD]; k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
k.CloseHandle.argtypes=[W.HANDLE]
h=k.OpenProcess(0x410,False,a.pid)
if not h: raise C.WinError(C.get_last_error())
def read(p,n):
 b=C.create_string_buffer(n); got=C.c_size_t(); k.ReadProcessMemory(h,p,b,n,C.byref(got)); return b.raw[:got.value]
def word(p):
 b=read(p,4); return struct.unpack('<I',b)[0] if len(b)==4 else 0
cs=Cs(CS_ARCH_X86,CS_MODE_32)
try:
 for s in a.object:
  p=int(s,16); vt=word(p); print(f'object={p:08X} vt={vt:08X}')
  for i in range(0,140 if 'dev' in s else 12):
   fn=word(vt+4*i); print(f' slot{i}={fn:08X} bytes={read(fn,16).hex()}')
 for s in a.code:
  p=int(s,16)
  print(f'CODE {p:08X}')
  for i in cs.disasm(read(p,120),p): print(f'{i.address:08X} {i.mnemonic} {i.op_str}')
finally:k.CloseHandle(h)
