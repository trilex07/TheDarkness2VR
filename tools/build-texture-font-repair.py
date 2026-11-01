"""Build a private asset repair from evidence and previously extracted fonts.

No installed files are modified. Compressed native BC2 has FOUR uint32 planes
per 16KB chunk; uncompressed archive entries use standard interleaved BC blocks.
"""
import argparse
import json
import math
from pathlib import Path
import struct

import numpy as np
from PIL import Image, ImageDraw
from polish_pack import Cache, require, sha


LETTERS = [('Ó','O','acute'),('ó','o','acute'),('Ą','A','ogonek'),('ą','a','ogonek'),
           ('Ć','C','acute'),('ć','c','acute'),('Ę','E','ogonek'),('ę','e','ogonek'),
           ('Ł','L','stroke'),('ł','l','stroke'),('Ń','N','acute'),('ń','n','acute'),
           ('Ś','S','acute'),('ś','s','acute'),('Ź','Z','acute'),('ź','z','acute'),
           ('Ż','Z','dot'),('ż','z','dot')]


def unplane(data, planes):
    """Native BC1:2 planes; BC2:4 planes. All plane elements are uint32."""
    require(len(data) % (planes*4) == 0, 'Block alignment')
    output = bytearray()
    for off in range(0, len(data), 16384):
        chunk = data[off:off+16384]
        output.extend(np.frombuffer(chunk, dtype='<u4').reshape(planes,-1).T.copy().tobytes())
    return bytes(output)


def unplane_bc3(data):
    require(len(data)%16==0,'BC3 block alignment')
    output=bytearray(len(data))
    for off in range(0,len(data),16384):
        chunk=data[off:off+16384];n=len(chunk)//16
        for plane,width,start in ((0,2,0),(1,6,2),(2,4,8),(3,4,12)):
            source=sum((2,6,4,4)[:plane])*n
            for block in range(n):
                output[off+block*16+start:off+block*16+start+width]=chunk[source+block*width:source+(block+1)*width]
    return bytes(output)


def counts(body):
    found=[]
    for n in range(50,300):
        if n*46 > len(body): break
        codes = struct.unpack_from('<'+str(n)+'H',body,n*44)
        if 32 in codes and 65 in codes and all(a<b for a,b in zip(codes,codes[1:])):
            found.append((n,codes))
    require(len(found)==1, 'Ambiguous Latin font count')
    return found[0]


def encode_bc2(alpha):
    w,h=alpha.size
    a=np.asarray(alpha,dtype=np.uint8)
    blocks=a.reshape(h//4,4,w//4,4).transpose(0,2,1,3).reshape(-1,16)
    q=((blocks.astype(np.uint16)+8)//17).astype(np.uint8)
    result=np.zeros((len(blocks),16),dtype=np.uint8)
    result[:,:8]=q[:,0::2] | (q[:,1::2]<<4)
    result[:,8:10]=255
    return result.tobytes()


def graft(body, header, texheader, standard, proof, name):
    n,codes=counts(body)
    require(not any(ord(c) in codes for c,_,_ in LETTERS),'Partial Polish font requires separate review')
    suffix=texheader[-149:]
    require(suffix[0]==2,'Expected native BC2 format')
    w,h=struct.unpack_from('<HH',suffix,6)
    require(len(standard)==w*h,'BC2 atlas byte size')
    alpha=Image.frombytes('RGBA',(w,h),standard,'bcn',2).getchannel('A')
    maxheight=max(round((struct.unpack_from('<4f',body,codes.index(ord(d))*16)[3]-
                         struct.unpack_from('<4f',body,codes.index(ord(d))*16)[1])*h) for _,d,_ in LETTERS)
    rowheight=maxheight+12
    tiles=[];x=4;y=h+4
    for ch,donor,kind in LETTERS:
        i=codes.index(ord(donor));uv=struct.unpack_from('<4f',body,i*16)
        x0,y0,x1,y1=round(uv[0]*w),round(uv[1]*h),round(uv[2]*w),round(uv[3]*h)
        crop=alpha.crop((x0,y0,x1,y1))
        tile=Image.new('L',(crop.width+8,crop.height+8));tile.paste(crop,(4,4))
        require(tile.width<w-8,'Glyph too wide')
        if x+tile.width+4>w:x=4;y+=rowheight
        ink=tile.getbbox();require(ink is not None,'Empty donor')
        left,top,right,bottom=ink;mid=(left+right)//2
        draw=ImageDraw.Draw(tile);thickness=2 if crop.height>=28 else 1
        if kind=='acute':draw.line((mid-1,max(0,top-2),mid+3,max(0,top-6)),fill=255,width=thickness)
        elif kind=='dot':draw.rectangle((mid-1,max(0,top-5),mid+1,max(0,top-3)),fill=255)
        elif kind=='ogonek':draw.line((right-2,bottom-1,right-3,bottom+2,right,bottom+3),fill=255,width=thickness)
        else:draw.line((max(0,left-2),max(top,(top+bottom)//2+2),min(tile.width-1,right+2),(top+bottom)//2-2),fill=255,width=thickness)
        require(tile.tobytes()!=Image.fromarray(np.pad(np.asarray(crop),4)).tobytes(),'Accent was not drawn')
        tiles.append((ord(ch),i,x,y,tile));x+=tile.width+4
    nh=1 << (max(h+1,y+rowheight+4)-1).bit_length()
    enlarged=Image.new('L',(w,nh));enlarged.paste(alpha,(0,0))
    records={}
    for i,code in enumerate(codes):
        u0,v0,u1,v1=struct.unpack_from('<4f',body,i*16)
        records[code]=(struct.pack('<4f',u0,v0*h/nh,u1,v1*h/nh),body[n*16+i*28:n*16+(i+1)*28])
    banner=Image.new('L',(sum(t.width+4 for _,_,_,_,t in tiles),rowheight));bx=0
    for code,i,x,y,tile in tiles:
        enlarged.paste(tile,(x,y));banner.paste(tile,(bx,0));bx+=tile.width+4
        records[code]=(struct.pack('<4f',x/w,y/nh,(x+tile.width)/w,(y+tile.height)/nh),body[n*16+i*28:n*16+(i+1)*28])
    keys=sorted(records);nn=len(keys)
    newbody=b''.join(records[k][0] for k in keys)+b''.join(records[k][1] for k in keys)+struct.pack('<'+str(nn)+'H',*keys)+body[n*46:]
    newheader=bytearray(header)
    offsets=[i for i in range(len(header)-35) if header[i:i+4]==struct.pack('<I',n) and header[i+32:i+36]==struct.pack('<I',n)]
    require(offsets,'Missing native language count arrays')
    countoff=min(offsets)
    for off in (countoff,countoff+32):struct.pack_into('<I',newheader,off,nn)
    th=bytearray(texheader)
    # Native suffix repeats base/full dimensions at offsets2/4 and6/8.
    require(struct.unpack_from('<HH',suffix,2)==(w,h),'Unexpected texture dimensions')
    for off in (4,8):struct.pack_into('<H',th,len(th)-149+off,nh)
    # Keep original block data exact; only new rows are encoded.
    encoded=standard+encode_bc2(enlarged.crop((0,h,w,nh)))
    decoded=Image.frombytes('RGBA',(w,nh),encoded,'bcn',2).getchannel('A')
    require(decoded.crop((0,0,w,h)).tobytes()==alpha.tobytes(),'Original atlas changed')
    for code,i,x,y,tile in tiles:
        require(decoded.crop((x,y,x+tile.width,y+tile.height)).getbbox() is not None,'Missing encoded glyph')
    require(newbody[nn*46:]==body[n*46:],'Auxiliary languages changed')
    for i,code in enumerate(codes):
        j=keys.index(code)
        require(newbody[nn*16+j*28:nn*16+(j+1)*28]==body[n*16+i*28:n*16+(i+1)*28],'Original metrics changed')
    banner.save(proof/(name+'-polish.png'))
    return bytes(newheader),newbody,bytes(th),encoded,dict(font=name,count_before=n,count_after=nn,header_offset=countoff,old_size=[w,h],new_size=[w,nh])


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache',type=Path,required=True)
    ap.add_argument('--textures',type=Path,required=True)
    ap.add_argument('--extract',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    a=ap.parse_args();require(not a.out.exists(),'Fresh output required')
    a.out.mkdir(parents=True);proof=a.out/'proof';proof.mkdir()
    replacements={s:{} for s in ('H.Texture','B.Texture','F.Texture','H.Font','B.Font','H.Texture_en','B.Texture_en')}
    tm=json.loads((a.textures/'manifest.json').read_text())
    require(len(tm['items'])==64 and all(i['format'] in ('DXT1','DXT5') for i in tm['items']),'Expected pinned BC1/BC3 package')
    for item in tm['items']:
        rel=item['path']
        for stem,tag in [('H.Texture','h'),('B.Texture','b'),('F.Texture','f')]:
            if item[tag+'_size']:
                p=a.textures/tag.upper()/rel;require(sha(p)==item[tag+'_sha256'],'Input texture hash')
                data=p.read_bytes()
                replacements[stem][rel]=data if tag=='h' else (unplane(data,2) if item['format']=='DXT1' else unplane_bc3(data))
    src=Cache(a.cache,'B.Texture_en');headers=Cache(a.cache,'H.Texture_en')
    reports=[]
    for hp in sorted((a.extract/'H.Font').rglob('*')):
        if not hp.is_file():continue
        rel=hp.relative_to(a.extract/'H.Font').as_posix()
        if '/Fonts/' not in rel and not rel.startswith('Temp/FontCache/'):continue
        body=(a.extract/'B.Font'/rel).read_bytes()
        try:n,codes=counts(body)
        except RuntimeError:raise
        except ValueError:continue  # Japanese-only fonts have no Latin language0.
        if all(ord(c) in codes for c,_,_ in LETTERS):continue
        matches=[p for p in src.entries if p.endswith('/'+hp.name+'Glyph_en_f.tga')]
        require(len(matches)==1,'Atlas identity: '+rel);tex=matches[0]
        raw=(a.extract/'B.Texture_en'/tex).read_bytes()
        _,_,packed,size=src.entries[tex]
        standard=raw if packed==size else unplane(raw,4)
        th=(a.extract/'H.Texture_en'/tex).read_bytes()
        h,b,t,d,report=graft(body,hp.read_bytes(),th,standard,proof,hp.name)
        replacements['H.Font'][rel]=h;replacements['B.Font'][rel]=b
        replacements['H.Texture_en'][tex]=t;replacements['B.Texture_en'][tex]=d
        reports.append(report)
        print('Patched',rel,report,flush=True)
    require(len(reports)==19,'Expected nineteen omitted Latin fonts')
    before={};after={}
    for stem,reps in replacements.items():
        Cache(a.cache,stem).write(a.out/'payload',reps)
        for ext in ('.cache','.toc'):
            name=stem+ext;before[name]=sha(a.cache/name);after[name]=sha(a.out/'payload'/name)
    manifest=dict(schema=1,kind='texture-font-repair',before=before,files=after,fonts=reports,
                  textures=64,tex_input_sha256=sha(a.textures/'manifest.json'))
    (a.out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('BUILD_VERIFIED',a.out,flush=True)


if __name__=='__main__':main()
