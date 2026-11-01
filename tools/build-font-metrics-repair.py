"""Normalize previously shipped six Polish font atlases to native donor cells."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image,ImageDraw
from polish_pack import Cache,require,sha

spec=importlib.util.spec_from_file_location('build_base',Path(__file__).with_name('build-texture-font-repair.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache',type=Path,required=True)
    ap.add_argument('--extract',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    a=ap.parse_args();require(not a.out.exists(),'Fresh output required');a.out.mkdir(parents=True)
    proof=a.out/'proof';proof.mkdir();reps={'B.Font':{},'B.Texture_en':{}};report=[]
    fonts=['EE/Menus/Fonts/SubtitlesFont','EE/Menus/Fonts/MenuFont']+['Temp/FontCache/darknessheader_'+str(i)+'.syn' for i in (19,24,26,32)]
    texcache=Cache(a.cache,'B.Texture_en')
    for rel in fonts:
        name=Path(rel).name;body=(a.extract/'B.Font'/rel).read_bytes();n,codes=r.counts(body)
        tex=next(p for p in texcache.entries if p.endswith('/'+name+'Glyph_en_f.tga'))
        header=(a.extract/'H.Texture_en'/tex).read_bytes();w,h=struct.unpack_from('<HH',header[-149:],6)
        standard=(a.extract/'B.Texture_en'/tex).read_bytes();require(len(standard)==w*h,'BC2 atlas size')
        original=Image.frombytes('RGBA',(w,h),standard,'bcn',2).getchannel('A');alpha=np.array(original);newbody=bytearray(body);tiles=[]
        original_uvs={c:body[i*16:(i+1)*16] for i,c in enumerate(codes)}
        for ch,donor,kind in r.LETTERS:
            i=codes.index(ord(donor));j=codes.index(ord(ch));uv=struct.unpack_from('<4f',body,i*16);slot=struct.unpack_from('<4f',body,j*16)
            box=tuple(round(v*s) for v,s in zip(uv,(w,h,w,h)));sb=tuple(round(v*s) for v,s in zip(slot,(w,h,w,h)))
            tile=original.crop(box);require(tile.width<=sb[2]-sb[0] and tile.height<=sb[3]-sb[1],'Donor must fit existing slot')
            bounds=tile.getbbox();require(bounds,'Empty donor');left,top,right,bottom=bounds;mid=(left+right)//2;d=ImageDraw.Draw(tile)
            if kind=='acute':d.line((mid-1,max(0,top-2),mid+2,max(0,top-5)),fill=255,width=1)
            elif kind=='dot':d.rectangle((mid-1,max(0,top-4),mid+1,max(0,top-2)),fill=255)
            elif kind=='ogonek':d.line((max(0,right-2),bottom-1,max(0,right-3),min(tile.height-1,bottom+2),min(tile.width-1,right),min(tile.height-1,bottom+3)),fill=255,width=1)
            else:d.line((max(0,left-1),(top+bottom)//2+2,min(tile.width-1,right+1),(top+bottom)//2-2),fill=255,width=1)
            x,y=sb[:2];alpha[sb[1]:sb[3],sb[0]:sb[2]]=0;alpha[y:y+tile.height,x:x+tile.width]=np.array(tile)
            dx=x-box[0];dy=y-box[1]
            newuv=(uv[0]+dx/w,uv[1]+dy/h,uv[2]+dx/w,uv[3]+dy/h)
            struct.pack_into('<4f',newbody,j*16,*newuv)
            newbody[n*16+j*28:n*16+(j+1)*28]=body[n*16+i*28:n*16+(i+1)*28]
            require(newuv[2]-newuv[0]==uv[2]-uv[0] and newuv[3]-newuv[1]==uv[3]-uv[1],'UV span mismatch')
            tiles.append(tile)
        fresh=r.encode_bc2(Image.fromarray(alpha));encoded=bytearray(standard)
        for off in range(0,len(encoded),16):encoded[off:off+8]=fresh[off:off+8]
        decoded=Image.frombytes('RGBA',(w,h),encoded,'bcn',2).getchannel('A')
        for i,c in enumerate(codes):
            if chr(c) in [v[0] for v in r.LETTERS]:continue
            require(newbody[i*16:(i+1)*16]==original_uvs[c],'Original UV changed')
            uv=struct.unpack_from('<4f',body,i*16);box=tuple(round(v*s) for v,s in zip(uv,(w,h,w,h)))
            require(decoded.crop(box).tobytes()==original.crop(box).tobytes(),'Original glyph pixels changed: '+name+' '+str(c))
        require(newbody[n*46:]==body[n*46:],'Auxiliary languages changed')
        banner=Image.new('L',(sum(t.width+2 for t in tiles),max(t.height for t in tiles)));x=0
        for t in tiles:banner.paste(t,(x,0));x+=t.width+2
        banner.save(proof/(name+'-polish.png'))
        reps['B.Font'][rel]=bytes(newbody);reps['B.Texture_en'][tex]=bytes(encoded);report.append(name)
        print('Native donor metrics and spans verified:',name,flush=True)
    before={};after={}
    for stem,patches in reps.items():
        Cache(a.cache,stem).write(a.out/'payload',patches)
        for ext in ('.cache','.toc'):
            name=stem+ext;before[name]=sha(a.cache/name);after[name]=sha(a.out/'payload'/name)
    (a.out/'manifest.json').write_text(json.dumps(dict(kind='font-metrics-repair',before=before,files=after,fonts=report),indent=2),encoding='utf-8')
    print('BUILD_VERIFIED',a.out)


if __name__=='__main__':main()
