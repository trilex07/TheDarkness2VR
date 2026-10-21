"""Read-only x86 minidump exception, EBP chain and candidate stack addresses.
No symbol guesses: stack scans are explicitly distinguished from EBP frames.
"""
import argparse, struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ap=argparse.ArgumentParser(); ap.add_argument('dump'); args=ap.parse_args()
d=Path(args.dump).read_bytes()
def unpack(fmt, off): return struct.unpack_from('<'+fmt,d,off)
count,directory=unpack('II',8)
streams={unpack('III',directory+i*12)[0]:unpack('III',directory+i*12)[1:] for i in range(count)}
regions=[]; modules=[]; timestamps={}; images={}
if 4 in streams:
    _,r=streams[4]
    for i in range(unpack('I',r)[0]):
        p=r+4+i*108; base,size=unpack('QI',p); name=unpack('I',p+20)[0]
        length=unpack('I',name)[0]; text=d[name+4:name+4+length].decode('utf-16-le')
        modules.append((base,base+size,text))
        timestamps[base]=unpack('I',p+16)[0]
if 5 in streams:
    _,r=streams[5]
    for i in range(unpack('I',r)[0]):
        addr,size,off=unpack('QII',r+4+i*16); regions.append((addr,addr+size,off))
if 9 in streams:
    _,r=streams[9]; n,off=unpack('QQ',r)
    for i in range(n):
        addr,size=unpack('QQ',r+16+i*16); regions.append((addr,addr+size,off)); off+=size
def read(addr,n):
    for start,end,off in regions:
        if start<=addr and addr+n<=end: return d[off+addr-start:off+addr-start+n]
    for start,end,name in modules:
        if start<=addr and addr+n<=end:
            if start not in images:
                import pefile
                path=Path(name.replace('Windows\\System32','Windows\\SysWOW64'))
                candidates=[path]
                if str(path).lower().endswith('darkness ii\\d3d9.dll'):
                    candidates+=list(path.parent.glob('d3d9.dll*'))
                    candidates+=list(Path('C:/Users/Trilex/AppData/Local/Darkness2VR/backups').glob('*.dll'))
                images[start]=None
                for candidate in candidates:
                    try:
                        pe=pefile.PE(str(candidate),fast_load=True)
                        if pe.FILE_HEADER.TimeDateStamp==timestamps[start] and pe.OPTIONAL_HEADER.SizeOfImage==end-start:
                            images[start]=pe; break
                    except Exception: pass
            pe=images[start]
            if pe: return pe.get_data(addr-start,n)
    return b''
def word(addr):
    b=read(addr,4); return struct.unpack('<I',b)[0] if len(b)==4 else 0
def label(addr):
    return next((f'{name}+0x{addr-start:X}' for start,end,name in modules if start<=addr<end),f'0x{addr:X}')
_,r=streams[6]; tid=unpack('I',r)[0]; code=unpack('I',r+8)[0]; address=unpack('Q',r+24)[0]
size,ctx=unpack('II',r+160)
regs={name:unpack('I',ctx+off)[0] for name,off in [('edi',156),('esi',160),('ebx',164),('edx',168),('ecx',172),('eax',176),('ebp',180),('eip',184),('esp',196)]}
print(f'EXCEPTION thread={tid} code={code:08X} at {label(address)} contextSize={size}')
print(' '.join(f'{k}={v:08X}' for k,v in regs.items()))
cs=Cs(CS_ARCH_X86,CS_MODE_32)
for i in cs.disasm(read(regs['eip'],48),regs['eip']): print(f'{i.address:08X} {i.mnemonic} {i.op_str}')
print('EBP chain (unverified without symbols/unwind metadata):')
fp=regs['ebp']
for i in range(25):
    nxt,ret=word(fp),word(fp+4); print(f'  fp={fp:08X} ret={ret:08X} {label(ret)}')
    if not fp<nxt<fp+0x100000: break
    fp=nxt
print('Stack scan candidates (NOT an unwound call stack):')
for offset in range(0,512,4):
    a=regs['esp']+offset; v=word(a)
    if any(start<=v<end for start,end,_ in modules): print(f'  {a:08X}: {v:08X} {label(v)}')
print('Modules:')
for start,end,name in modules: print(f'{start:08X}-{end:08X} {name}')
from collections import Counter
for start,end,off in regions:
    if start<=regs['esp']<end:
        raw=read(regs['esp'],min(end-regs['esp'],0x400000))
        counts=Counter(v[0] for v in struct.iter_unpack('<I',raw[:len(raw)//4*4])
                       if any(a<=v[0]<b for a,b,_ in modules))
        print('Repeated module pointers on exception stack (candidates only):')
        for addr,n in counts.most_common(12):
            print(n,label(addr))
            for op in cs.disasm(read(addr-16,32),addr-16): print(f'    {op.address:08X} {op.mnemonic} {op.op_str}')
        break
