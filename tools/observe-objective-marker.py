"""MAIN read-only objective/camera recorder. No hooks, input injection or game writes.
Supply the exact built DLL and matching MAP from the installed receipt.
"""
import argparse,ctypes as c,hashlib,json,math,re,struct,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--pid',type=int,required=True)
p.add_argument('--base',type=lambda s:int(s,0),required=True)
p.add_argument('--dll',type=Path,required=True)
p.add_argument('--map',type=Path,required=True)
p.add_argument('--sha256',required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--seconds',type=float,default=30)
a=p.parse_args()
assert 1<=a.seconds<=180
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(a.dll)==a.sha256.upper(),'Local DLL is not the requested receipt'
api=c.WinDLL('kernel32',use_last_error=True)
api.OpenProcess.argtypes=[c.c_ulong,c.c_int,c.c_ulong];api.OpenProcess.restype=c.c_void_p
api.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
api.CloseHandle.argtypes=[c.c_void_p]
api.QueryFullProcessImageNameW.argtypes=[c.c_void_p,c.c_ulong,c.c_wchar_p,c.POINTER(c.c_ulong)]
h=api.OpenProcess(0x1010,False,a.pid)
assert h,'Read-only OpenProcess failed'
def read(address,size):
    assert 0x10000<=address<=0xffffffff-size and 0<size<=4096
    out=c.create_string_buffer(size);got=c.c_size_t()
    assert api.ReadProcessMemory(h,address,out,size,c.byref(got)) and got.value==size
    return out.raw
def word(address):return struct.unpack('<I',read(address,4))[0]
def floats(address,count=16):
    values=struct.unpack('<'+'f'*count,read(address,count*4))
    assert all(math.isfinite(v) for v in values)
    return values
symbols={}
for line in a.map.read_text().splitlines():
    m=re.search(r'\?([A-Za-z0-9_]+)@.*?\s([0-9a-fA-F]{8})\s+',line)
    if m:symbols[m[1]]=a.base+int(m[2],16)-0x10000000
def raw(name,size):return read(symbols[name],size).hex()
def flag(name):return read(symbols[name],1)[0]
def optional(fn):
    try:return fn()
    except (AssertionError,KeyError,ValueError,struct.error):return None
try:
    path=c.create_unicode_buffer(32768);length=c.c_ulong(32768)
    assert api.QueryFullProcessImageNameW(h,0,path,c.byref(length))
    exe=Path(path.value)
    assert exe.name.lower()=='darknessii.exe'
    assert sha(exe)=='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
    assert sha(exe.with_name('d3d9.dll'))==a.sha256.upper()
    local=a.dll.read_bytes();nt=struct.unpack_from('<I',local,0x3c)[0]
    assert read(a.base,2)==b'MZ' and word(a.base+0x3c)==nt
    assert read(a.base+nt,12)==local[nt:nt+12]
    assert read(a.base+nt+0x50,4)==local[nt+0x50:nt+0x54]
    # GetGameState: 43D030 reads rules+3D0. Never invoke its native method.
    assert read(0x43d030,6)==bytes.fromhex('8b89d0030000')
    def camera_snapshot():
        camera=word(symbols['g_nativeGameplay']+12)
        if not camera:return None
        return dict(camera=camera,table=word(camera),native_world=floats(camera+0x80),
                    native_view_projection=floats(camera+0x500))
    def state_snapshot():
        manager=word(symbols['g_nativeRegionManager'])
        assert word(manager)==0xfc7d00
        rules=word(word(manager+0xbc))
        assert word(rules)==0x1008248
        state=word(word(rules+0x3d0));table=word(state)
        count_method,item_method=word(table+0x78),word(table+0x7c)
        targets=[]
        # Live FD8568 type and the two complete consumer instructions corroborate
        # vector+68/+6C, stride24, reference at entry+4. Never call the methods.
        if table==0xfd8568 and count_method==0xcba9d0 and item_method==0xb69750:
            assert read(0xcba9d0,6)==bytes.fromhex('8b516c2b5168')
            assert read(0xb69754,10)==bytes.fromhex('8b49688d04408b4cc104')
            begin,end=struct.unpack('<2I',read(state+0x68,8))
            assert end>=begin and (end-begin)%24==0 and end-begin<=24*64
            for entry in range(begin,end,24):
                ref=word(entry+4);obj=word(ref)
                targets.append(dict(entry=entry,reference=ref,object=obj,
                    table=word(obj) if obj else None,prefix=read(obj,512).hex() if obj else None))
        return dict(manager=manager,rules=rules,state=state,table=table,
                    prefix=read(state,1024).hex(),count_method=count_method,
                    item_method=item_method,targets=targets)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    with a.out.open('w',encoding='utf-8') as out:
        out.write(json.dumps(dict(kind='identity',pid=a.pid,base=a.base,sha256=a.sha256,asynchronous=True))+'\n')
        for _ in range(round(a.seconds*10)):
            row=dict(time=time.time(),xr_state=word(symbols['g_xrSessionState']),
                world=flag('g_in3DGameplayMode'),native_cinema=flag('g_nativeCinematicPlaying'),
                video=flag('g_binkVideo'),head_valid=flag('g_headDeltaValid'),
                head_delta=optional(lambda:floats(symbols['g_headDeltaView'])),
                native_gameplay=raw('g_nativeGameplay',56),
                eye_params=raw('g_frameEyeParams',344),
                camera=optional(camera_snapshot),game_state=optional(state_snapshot))
            out.write(json.dumps(row)+'\n');out.flush();time.sleep(.1)
    print('Recorded read-only objective/camera state:',a.out)
finally:api.CloseHandle(h)
