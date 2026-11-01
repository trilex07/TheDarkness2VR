"""Font-only candidate: add glyphs without resizing atlases or moving old UVs."""
import importlib.util
from pathlib import Path
import struct

import numpy as np
from PIL import Image,ImageDraw

spec=importlib.util.spec_from_file_location('repair',Path(__file__).with_name('build-texture-font-repair.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)


def graft(body,header,texheader,standard,proof,name):
    n,codes=r.counts(body);w,h=struct.unpack_from('<HH',texheader[-149:],6)
    r.require(len(standard)==w*h and texheader[-149]==2,'BC2 atlas')
    original=Image.frombytes('RGBA',(w,h),standard,'bcn',2).getchannel('A')
    alpha=np.array(original)
    mask=alpha>0
    for i in range(n):
        u0,v0,u1,v1=struct.unpack_from('<4f',body,i*16)
        x0,y0=max(0,int(np.floor(u0*w))-1),max(0,int(np.floor(v0*h))-1)
        x1,y1=min(w,int(np.ceil(u1*w))+1),min(h,int(np.ceil(v1*h))+1)
        mask[y0:y1,x0:x1]=True
    records={c:(body[i*16:(i+1)*16],body[n*16+i*28:n*16+(i+1)*28]) for i,c in enumerate(codes)}
    tiles=[]
    for ch,donor,kind in r.LETTERS:
        i=codes.index(ord(donor));uv=struct.unpack_from('<4f',body,i*16)
        crop=original.crop(tuple(round(v*s) for v,s in zip(uv,(w,h,w,h))))
        bbox=crop.getbbox();r.require(bbox,'Empty donor');ink=crop.crop(bbox)
        tile=Image.new('L',(ink.width+4,ink.height+7));tile.paste(ink,(2,4))
        draw=ImageDraw.Draw(tile);mid=tile.width//2;bottom=4+ink.height
        if kind=='acute':draw.line((mid-1,3,mid+2,0),fill=255,width=1)
        elif kind=='dot':draw.rectangle((mid-1,0,mid+1,2),fill=255)
        elif kind=='ogonek':draw.line((tile.width-5,bottom-1,tile.width-6,bottom+2,tile.width-3,bottom+3),fill=255,width=1)
        else:draw.line((0,4+ink.height//2+2,tile.width-1,4+ink.height//2-2),fill=255,width=1)
        tw,th=tile.size
        integral=np.pad(mask.astype(np.int32),((1,0),(1,0))).cumsum(0).cumsum(1)
        sums=integral[th:,tw:]-integral[:-th,tw:]-integral[th:,:-tw]+integral[:-th,:-tw]
        ys,xs=np.where(sums==0)
        if not len(xs) and name=='DebugFont':
            records[ord(ch)]=records[ord(donor)]
            continue  # Debug-only fallback; preserve its compact original atlas.
        r.require(len(xs)>0,'No safe unchanged-size atlas space: '+name+' '+ch)
        x,y=int(xs[0]),int(ys[0]);alpha[y:y+th,x:x+tw]=np.array(tile)
        mask[max(0,y-1):min(h,y+th+1),max(0,x-1):min(w,x+tw+1)]=True
        records[ord(ch)]=(struct.pack('<4f',x/w,y/h,(x+tw)/w,(y+th)/h),body[n*16+i*28:n*16+(i+1)*28])
        tiles.append(tile)
    keys=sorted(records);nn=len(keys)
    newbody=b''.join(records[k][0] for k in keys)+b''.join(records[k][1] for k in keys)+struct.pack('<'+str(nn)+'H',*keys)+body[n*46:]
    offsets=[i for i in range(len(header)-35) if header[i:i+4]==struct.pack('<I',n) and header[i+32:i+36]==struct.pack('<I',n)]
    r.require(offsets,'Missing paired count arrays');off=min(offsets)
    newheader=bytearray(header)
    for p in (off,off+32):struct.pack_into('<I',newheader,p,nn)
    # Replace only alpha bytes, keeping all native BC2 color endpoints/indices.
    encoded=bytearray(standard);fresh=r.encode_bc2(Image.fromarray(alpha))
    for j in range(0,len(encoded),16):encoded[j:j+8]=fresh[j:j+8]
    decoded=Image.frombytes('RGBA',(w,h),encoded,'bcn',2).getchannel('A')
    for i,c in enumerate(codes):
        j=keys.index(c)
        r.require(newbody[j*16:(j+1)*16]==body[i*16:(i+1)*16],'Original UV changed')
        r.require(newbody[nn*16+j*28:nn*16+(j+1)*28]==body[n*16+i*28:n*16+(i+1)*28],'Original metrics changed')
        uv=struct.unpack_from('<4f',body,i*16);box=tuple(round(v*s) for v,s in zip(uv,(w,h,w,h)))
        r.require(decoded.crop(box).tobytes()==original.crop(box).tobytes(),'Original glyph pixels changed')
    r.require(newbody[nn*46:]==body[n*46:],'Other languages changed')
    banner=Image.new('L',(sum(t.width+3 for t in tiles),max(t.height for t in tiles)));x=0
    for tile in tiles:banner.paste(tile,(x,0));x+=tile.width+3
    banner.save(proof/(name+'-polish.png'))
    return bytes(newheader),newbody,texheader,bytes(encoded),dict(font=name,count_before=n,count_after=nn,header_offset=off,old_size=[w,h],new_size=[w,h],original_uvs='exact',original_glyph_pixels='exact')


r.graft=graft
if __name__=='__main__':r.main()
