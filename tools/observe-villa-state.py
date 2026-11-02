"""MAIN read-only snapshot of this exact candidate's mapped globals; no writes."""
import argparse,ctypes as c,hashlib,json,re,struct,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--pid',type=int,required=True)
p.add_argument('--base',type=lambda s:int(s,0),required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--avatar',type=lambda s:int(s,0))
a=p.parse_args()
root=Path(__file__).resolve().parent.parent
expected='FD6470F6B135A49DD77E30A023A5A931683025C719455EC3EAAA7220A01443AF'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(root/'d3d9.dll')==expected
api=c.WinDLL('kernel32',use_last_error=True)
api.OpenProcess.argtypes=[c.c_ulong,c.c_int,c.c_ulong];api.OpenProcess.restype=c.c_void_p
api.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
api.CloseHandle.argtypes=[c.c_void_p]
api.QueryFullProcessImageNameW.argtypes=[c.c_void_p,c.c_ulong,c.c_wchar_p,c.POINTER(c.c_ulong)]
h=api.OpenProcess(0x1010,False,a.pid)
assert h,'OpenProcess read-only failed'
def read(address,size):
    out=c.create_string_buffer(size);got=c.c_size_t()
    assert api.ReadProcessMemory(h,address,out,size,c.byref(got)) and got.value==size
    return out.raw
def word(address):return struct.unpack('<I',read(address,4))[0]
symbols={}
for line in (root/'d3d9.map').read_text().splitlines():
    m=re.search(r'\?([A-Za-z0-9_]+)@.*?\s([0-9a-fA-F]{8})\s+',line)
    if m:symbols[m[1]]=a.base+int(m[2],16)-0x10000000
def raw(name,n):return read(symbols[name],n).hex()
try:
    path=c.create_unicode_buffer(32768);length=c.c_ulong(32768)
    assert api.QueryFullProcessImageNameW(h,0,path,c.byref(length))
    exe=Path(path.value)
    assert exe.name.lower()=='darknessii.exe'
    assert sha(exe)=='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
    assert sha(exe.with_name('d3d9.dll'))==expected
    # Verify module mapped PE headers against the exact locally built candidate.
    local=(root/'d3d9.dll').read_bytes();nt=struct.unpack_from('<I',local,0x3c)[0]
    assert read(a.base,2)==b'MZ' and word(a.base+0x3c)==nt
    assert read(a.base+nt,12)==local[nt:nt+12] # PE identity/machine/timestamp
    assert read(a.base+nt+0x50,4)==local[nt+0x50:nt+0x54] # SizeOfImage
    rows=[]
    def reference(address):
        try:
            ref=word(address)
            return dict(reference=ref,object=word(ref) if ref else None)
        except AssertionError:return dict(error='unreadable')
    def avatar_snapshot():
        if not a.avatar:return None
        av=a.avatar
        result=dict(avatar=av,table=word(av),camera=reference(av+0x62c),inventory=reference(av+0x630),model=reference(av+0x1f8c))
        camera=result['camera'].get('object')
        if camera:result['camera_table']=word(camera)
        assert result['table']==0x1001630
        inv=result['inventory'].get('object')
        if inv:
            result['inventory_table']=word(inv)
            assert result['inventory_table']==0xffca10
            result['slots']={label:reference(inv+offset) for label,offset in [('main',0xb0),('off',0xbc),('demon',0x20c+6*4)]}
        return result
    for i in range(30):
        flags={s:read(symbols[s],1)[0] for s in ('g_binkVideo','g_nativeCinematicPlaying','g_in3DGameplayMode','g_haveViews','g_headGeometry','g_headGeometryReferenceValid','g_protectedHandsPrivateReady','g_imagePreservingInput','g_protectedGestureActions','g_protectedAimPrivateReady','g_protectedTrackedHands')}
        row=dict(time=time.time(),flags=flags,bink_handles=list(struct.unpack('<16I',read(symbols['g_binkPlaying'],64))),
                 selected_resource=word(symbols['g_nativeHandsSelectedResource']),pose_camera=word(symbols['g_protectedHandsPoseCamera']),
                 native_gameplay_raw=raw('g_nativeGameplay',64),hand_poses_raw=raw('g_protectedHandPoses',160),
                 right_pause=read(symbols['g_protectedRightTentacle']+6,1)[0],left_pause=read(symbols['g_protectedLeftTentacle']+6,1)[0],avatar=avatar_snapshot(),
                 aim_table_raw=raw('g_protectedAimTable',28),tracked_frame_raw=raw('g_trackedFrame',112),tracked_hands_raw=raw('g_hands',224))
        row['bink_headers']={}
        for handle in row['bink_handles']:
            if handle:
                try:row['bink_headers'][hex(handle)]=read(handle,32).hex()
                except AssertionError:row['bink_headers'][hex(handle)]='UNREADABLE'
        rows.append(row);time.sleep(.1)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(dict(pid=a.pid,base=a.base,dll_sha256=expected,asynchronous=True,samples=rows),indent=2),encoding='utf-8')
    print(json.dumps(rows[-1],indent=2))
finally:api.CloseHandle(h)
