"""Build a private bounded SM3 warmup package from explicit files or traced hashes.
Does not install, execute, capture or redistribute shaders. FNV selects candidates;
full byte strings deduplicate entries. Match is not a live-bytecode identity proof.
"""
import argparse, hashlib, json, pathlib, re, struct
MAX_BYTES=4*1024*1024

def valid(raw):
    if len(raw)%4 or not 8<=len(raw)<=32768:return False
    w=struct.unpack('<'+'I'*(len(raw)//4),raw)
    if w[0] not in (0xfffe0300,0xffff0300):return False
    i=1
    while i<len(w):
        op=w[i]&65535
        if op==65535:return w[i]==65535 and i==len(w)-1
        n=(w[i]>>16)&32767 if op==65534 else (w[i]>>24)&15
        i+=n+1
    return False

def build(files):
    entries=[];seen=set();sources=[]
    for p in files:
        raw=p.read_bytes()
        if not valid(raw):raise ValueError('Unsupported or truncated SM3: '+str(p))
        if raw in seen:continue
        seen.add(raw);entries.append(struct.pack('<I',len(raw)//4)+raw)
        sources.append({'path':str(p),'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)})
    if not 1<=len(entries)<=256:raise ValueError('Require 1..256 unique shaders')
    package=struct.pack('<II',0x31575344,len(entries))+b''.join(entries)
    if len(package)>MAX_BYTES:raise ValueError('Package exceeds 4 MiB')
    return package,sources

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('files',type=pathlib.Path,nargs='*');ap.add_argument('--trace',type=pathlib.Path);ap.add_argument('--shader-dir',type=pathlib.Path);ap.add_argument('--output',type=pathlib.Path,required=True);a=ap.parse_args()
    files=a.files;missing=[]
    if bool(a.trace)!=bool(a.shader_dir) or (a.trace and files):ap.error('Use files OR --trace with --shader-dir')
    if a.trace:
        text=a.trace.read_text();wanted=set(re.findall(r'^ VS ([0-9A-F]{8})',text,re.M))|set(re.findall(r'^ PS .*hash=([0-9A-F]{8})',text,re.M));found=set();files=[]
        for p in sorted(a.shader_dir.rglob('*.bin')):
            h=2166136261
            for b in p.read_bytes():h=((h^b)*16777619)&0xffffffff
            if f'{h:08X}' in wanted:found.add(f'{h:08X}');files.append(p)
        missing=sorted(wanted-found)
    package,sources=build(files)
    targets=[a.output,a.output.with_suffix('.json')]
    if any(p.exists() for p in targets):ap.error('Output already exists; choose a fresh path')
    a.output.write_bytes(package);a.output.with_suffix('.json').write_text(json.dumps({'sha256':hashlib.sha256(package).hexdigest(),'bytes':len(package),'shaders':sources,'missing_trace_hashes':missing},indent=2)+'\n')
    print(f'Packaged {len(sources)} shaders / {len(package)} bytes; missing trace hashes={len(missing)}; NOT INSTALLED')
if __name__=='__main__':main()
