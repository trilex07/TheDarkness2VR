"""CPU fixture only: export exact local native functions to ignored evidence.

No process access, install, native target calls, generated game bytes in Git.
The x86 fixture relocates constant reads and the event-queue call only.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--com-receipt',type=Path)
    a=p.parse_args()
    # Minimal PE reader; a whole-file identity is evidence, method/slot bytes
    # are the actual compatibility gates (also independently checked in DLL).
    data=a.exe.read_bytes()
    pe=struct.unpack_from('<I',data,0x3c)[0]
    count=struct.unpack_from('<H',data,pe+6)[0]
    opt=pe+24;section_table=opt+struct.unpack_from('<H',data,pe+20)[0]
    base=struct.unpack_from('<I',data,opt+28)[0]
    def read(va,n):
        for i in range(count):
            vsize,rva,size,offset=struct.unpack_from('<IIII',data,section_table+40*i+8)
            if rva<=va-base and va-base+n<=rva+size:
                return data[offset+va-base-rva:offset+va-base-rva+n]
        raise ValueError(hex(va))
    methods=[('transform',0x6D56B0,0xB9,'a9e058beb6f77c70f5a36ad6b7e32c6030f63c02230d70f2f94874954be1c792'),
             ('position',0x591650,0x58,'6ae74f091f5ad4fcbbd483fef25dadc06726d5541e827d4fd17751f251f88e51'),
             ('updater',0xCB70C0,0x291,'e17dff6e0f5ab7232943a73943c1a3588e8ebd735e95fe040d50c2a26b7cebcd'),
             ('viewport',0xCEDC30,0x5D,'a0ef759f22b528a5d641546324725454c025e3742b4424d6b07dee21b90bd5ef'),
             ('mouse_getter',0x603190,0x1B,'e9c127f7d17c85159bc87d51c7aec652cb00fec8ab9114a7072931508bb045f2'),
             ('input_getter',0x43E160,0x17,'ace6e45967527155cce8426c6b949ac6dbd9e4e37d4eeb0b6dbc7b27e6418764'),
             ('mouse_poll',0x9A5D90,0x17A,'67e02e71a4e562cd5c8ce6e55708d4e1c3f15e65495187ffe9be3724cc170515'),
             ('mouse_constructor',0x821400,0x79,'abd98e7652244d6eb61ebac0a5f879044829789a86845f257631fca3f07511a5')]
    a.out.mkdir(parents=True,exist_ok=True)
    blobs={}
    for name,va,n,digest in methods:
        blob=read(va,n)
        if hashlib.sha256(blob).hexdigest()!=digest:raise ValueError('Changed '+name)
        blobs[name]=blob
    for name,tail in [('mouse_poll','c20400'),('mouse_getter','c20800'),
                      ('input_getter','c20800'),('mouse_constructor','c20800')]:
        if blobs[name][-3:]!=bytes.fromhex(tail):raise ValueError('Incomplete RET boundary '+name)
    for table,slot,target in [(0xFDA6E8,0x80,0xCB70C0),(0xFDA6E8,0x148,0x6D56B0),(0xFC00C8,0x6C,0x603190)]:
        if struct.unpack('<I',read(table+slot,4))[0]!=target:raise ValueError('Changed slot')
    for name in ('transform','position','mouse_getter'):(a.out/(name+'.bin')).write_bytes(blobs[name])
    reloc=[]
    for va,bits in [(0xFFBA08,0x3f000000),(0xF67A58,0x80000000),(0xFA8660,0x4b000000),(0xF72094,0x3f800000)]:
        if struct.unpack('<I',read(va,4))[0]!=bits:raise ValueError('Changed native rounding constant')
        needle=struct.pack('<I',va)
        if blobs['transform'].count(needle)!=1:raise ValueError('Ambiguous relocation')
        reloc.append((blobs['transform'].index(needle),bits))
    if blobs['position'][0x4d]!=0xe8 or 0x5916a2+struct.unpack_from('<i',blobs['position'],0x4e)[0]!=0x902ab0:
        raise ValueError('Event dispatch call')
    header='struct FixtureConstant {unsigned offset,bits;};\nstatic FixtureConstant constants[]={'+','.join('{%d,0x%08xu}'%x for x in reloc)+'};\n'
    (a.out/'fixture_contract.h').write_text(header)
    if a.com_receipt:
        receipt=json.loads(a.com_receipt.read_text())
        if receipt['status']!='READ_ONLY_EQUAL_PAIR' or receipt['loaded_methods']!=41 or len(receipt['raw']['snapshots'])!=2 or receipt['raw']['snapshots'][0]!=receipt['raw']['snapshots'][1]:
            raise ValueError('Not actual equal-pair mouse COM evidence')
        method=receipt['snapshot']['method'];code=bytes.fromhex(method['method_span_hex'])[5:]
        if len(code)!=96 or method['method_rva']!=0x823D0 or method['pe_timestamp']!=0x6AD92A09 or method['image_size']!=0x18C000 or hashlib.sha256(code).hexdigest()!=method['method_sha256']:
            raise ValueError('Changed external method receipt')
        (a.out/'external_com_fixture.h').write_text('static const uintptr_t overlayBase=0x%x;\nstatic const unsigned char overlayBytes[96]={%s};\n'%(method['module_base'],','.join('0x%02x'%v for v in code)))
    root=Path(__file__).resolve().parents[1]
    source=(root/'src/native_flash_cursor.inl').read_text()
    # Actual production callback/context implementation, no rewritten guards.
    callback=source[:source.index('static bool InstallNativeFlashCursor()')]
    binding='reinterpret_cast<FlashTransform>(0x6D56B0)'
    if callback.count(binding)!=1:raise ValueError('Changed original transform binding')
    # ASLR-safe CPU original-body relocation only; all actual guards, mapping,
    # publications and calling convention remain the production implementation.
    getter='reinterpret_cast<Getter>(0x603190)'
    if callback.count(getter)!=1:raise ValueError('Changed original getter binding')
    (a.out/'actual_flash_callback.inl').write_text(callback.replace(binding,'g_fixtureTransform').replace(getter,'g_fixtureGetter'))
    eye=(root/'src/eye_gaze.inl').read_text()
    start=eye.index('static bool ControllerMenuHit(')
    end=eye.index('static bool ControllerMenuConfirmReady()',start)
    (a.out/'actual_controller_hit.inl').write_text(eye[start:end])
    start=eye.index('struct MenuSurface {')
    (a.out/'actual_menu_surface.h').write_text(eye[start:eye.index('static SRWLOCK',start)])
    start=eye.index('static void RecordMenuSurface(')
    (a.out/'actual_record_menu_surface.inl').write_text(eye[start:eye.index('static bool g_menuClampSaved',start)])
    receipt=dict(exe_sha256=hashlib.sha256(data).hexdigest(),methods=methods,constant_relocations=reloc,
                 event_call_relocation_offset=0x4e,production_source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                 scope='CPU fake owners + actual transform/event bodies (original target rebased); no native updater execution or real XR/hover')
    (a.out/'fixture-inputs.json').write_text(json.dumps(receipt,indent=2))
    print('8 full native methods +3 slots +4 rounding constants PASS; mouse poll full RET04 boundary17A; fixture exported')


if __name__=='__main__':main()
