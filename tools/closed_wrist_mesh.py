"""Clip the SHA-pinned stock FP arms at their actual bind-pose wrists.

No asset bytes or textures are distributed. Reuse unused original VB/IB slots,
retain cooked buffer sizes, and close every cut contour in all five LODs.
"""
import collections
import math
import struct
import hands_only_asset as stock


def dot(a,b): return sum(x*y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mix(a,b,t): return tuple(x+(y-x)*t for x,y in zip(a,b))


def bind_origin(body,bone):
    m=struct.unpack_from('<16f',body,2+bone*64)
    stock.require(all(abs(m[i])<1e-6 for i in (3,7,11)) and abs(m[15]-1)<1e-6,
                  'Unexpected bind transform')
    for i in range(3):
        for j in range(3):
            stock.require(abs(dot(m[i*4:i*4+3],m[j*4:j*4+3])-(i==j))<1e-4,
                          'Nonrigid bind transform')
    return tuple(-dot(m[12:15],m[j*4:j*4+3]) for j in range(3))


def weights(vertex):
    return {bone:sum(vertex[36+i] for i in range(4) if vertex[32+i]==bone)
            for bone in vertex[32:36] if any(vertex[36+i] for i in range(4) if vertex[32+i]==bone)}


def hand_palette(header):
    # SHA-pinned cooked header: the HandRevised1 submesh has its own 34-entry
    # bone remap. Vertex UBYTE4 indices address THIS table, not the 42 bind
    # matrices/name records. The following sleeve table has 12 entries.
    at=2721
    stock.require(len(header)==2992 and struct.unpack_from('<I',header,at)[0]==34,
                  'Unexpected hand palette layout')
    palette=struct.unpack_from('<34I',header,at+4)
    stock.require(len(set(palette))==34 and all(i<42 for i in palette) and
                  struct.unpack_from('<I',header,at+4+34*4)[0]==12,
                  'Invalid hand palette remap')
    stock.require(palette.index(1)==19 and palette.index(26)==1,'Unexpected wrist remap')
    return palette


def blend(a,b,t,position):
    result=bytearray(a if t<.5 else b)
    struct.pack_into('<3f',result,0,*position)
    # Both UV sets are the stock half-float pairs. Preserve the original atlas.
    for offset in (24,28):
        uv=mix(struct.unpack_from('<2e',a,offset),struct.unpack_from('<2e',b,offset),t)
        stock.require(all(math.isfinite(x) for x in uv),'Invalid stock UV')
        struct.pack_into('<2e',result,offset,*uv)
    wa,wb=weights(a),weights(b)
    merged={k:wa.get(k,0)*(1-t)+wb.get(k,0)*t for k in wa.keys()|wb.keys()}
    selected=sorted(merged,key=lambda k:(-merged[k],k))[:4]
    total=sum(merged[k] for k in selected)
    stock.require(total>0,'Missing skin weights')
    values=[merged[k]*255/total for k in selected]
    quantized=[math.floor(v) for v in values]
    for i in sorted(range(len(values)),key=lambda i:values[i]-quantized[i],reverse=True)[:255-sum(quantized)]:
        quantized[i]+=1
    result[32:36]=bytes(selected+[0]*(4-len(selected)))
    result[36:40]=bytes(quantized+[0]*(4-len(selected)))
    return bytes(result)


def patch(header,body):
    output,hands,sleeves=stock.patch(header,body)
    # Skeleton order comes from the pinned header, not the live avatar's
    # different 130-bone numbering. Submesh vertex indices are remapped below.
    names=[];at=header.index(b'GAME_L1_TWIST2')-4
    for _ in range(42):
        n=struct.unpack_from('<I',header,at)[0];at+=4
        names.append(header[at:at+n].decode());at+=n
    stock.require([names[i] for i in (1,5,26,22)]==
                  ['GAME_L1_ARM3','GAME_L1_ARM2','GAME_R1_ARM3','GAME_R1_ARM2'],
                  'Unexpected native wrist palette')
    palette=hand_palette(header)
    planes=[]
    for wrist,elbow in ((1,5),(26,22)):
        origin=bind_origin(body,wrist);axis=sub(origin,bind_origin(body,elbow))
        length=math.sqrt(dot(axis,axis));stock.require(.05<length<.6,'Invalid forearm length')
        axis=tuple(x/length for x in axis)
        # Five millimetres toward the forearm leaves a short closed wrist cuff.
        planes.append((tuple(x-.005*d for x,d in zip(origin,axis)),axis))
    vertices=[body[stock.VB_OFFSET+i*40:stock.VB_OFFSET+(i+1)*40] for i in range(stock.VB_SIZE//40)]
    positions=[struct.unpack_from('<3f',v) for v in vertices]
    def plane(i): return 0 if positions[i][0]<0 else 1
    def distance(i):
        origin,axis=planes[plane(i)];return dot(sub(positions[i],origin),axis)
    distances=[distance(i) for i in range(len(vertices))]
    triangles=[];kept=set()
    for start,count in hands:
        tris=[struct.unpack_from('<3H',body,stock.IB_OFFSET+i*2) for i in range(start,start+count,3)]
        stock.require(all(len({plane(i) for i in tri})==1 for tri in tris),'Triangle crosses hands')
        triangles.append(tris)
        kept.update(i for tri in tris for i in tri if distances[i]>=0)
    # All LOD draw ranges include vertices 12..2147. Never introduce an index
    # outside a native hand draw's original min/max vertex bounds.
    free=iter(sorted(set(range(12,2148))-kept))
    allocated={};new_vertices={}
    def allocate(key,data):
        if key not in allocated:
            index=next(free,None);stock.require(index is not None,f'Insufficient original vertex capacity: allocated {len(allocated)}, kept {len(kept)}')
            allocated[key]=index;new_vertices[index]=data
        return allocated[key]
    def position(i):return struct.unpack_from('<3f',new_vertices.get(i,vertices[i]))
    def cut(a,b):
        key=tuple(sorted((a,b)))
        t=distances[a]/(distances[a]-distances[b]);p=mix(positions[a],positions[b],t)
        return allocate(('edge',)+key,blend(vertices[a],vertices[b],t,p))
    def point_key(i):return tuple(round(x,6) for x in position(i))
    result=bytearray(output);stats=[]
    for lod,(tris,(start,count)) in enumerate(zip(triangles,hands)):
        clipped=[];segments=[[],[]];removed=0
        for tri in tris:
            polygon=[];crossings=[]
            for a,b in zip(tri,tri[1:]+tri[:1]):
                inside=distances[a]>=0;next_inside=distances[b]>=0
                if inside:polygon.append(a)
                if inside!=next_inside:
                    v=cut(a,b);polygon.append(v);crossings.append(v)
            for j in range(1,len(polygon)-1):clipped.append((polygon[0],polygon[j],polygon[j+1]))
            if len(polygon)<3:removed+=1
            if crossings:
                stock.require(len(crossings)==2,'Ambiguous cut segment')
                segments[plane(tri[0])].append(tuple(crossings))
        cap_count=0
        for hand,edges in enumerate(segments):
            graph=collections.defaultdict(set);representative={}
            for a,b in edges:
                ka,kb=point_key(a),point_key(b)
                stock.require(ka!=kb,'Collapsed cut segment')
                graph[ka].add(kb);graph[kb].add(ka);representative[ka]=a;representative[kb]=b
            stock.require(len(graph)>=3 and all(len(v)==2 for v in graph.values()),'Cut is not a closed manifold loop')
            first=min(graph);loop=[first];previous=None;current=first
            while True:
                nxt=min(graph[current]-({previous} if previous is not None else set()))
                if nxt==first:break
                stock.require(nxt not in loop,'Repeated contour vertex')
                loop.append(nxt);previous,current=current,nxt
            stock.require(len(loop)==len(graph),'Multiple cut contours')
            ids=[representative[k] for k in loop]
            center=planes[hand][0]
            # Use the wrist's SUBMESH palette index. Skeleton index 1/26 here
            # selected the other wrist/a finger and stretched the cap between
            # independently tracked hands. Stock border weights stay unchanged.
            center_data=bytearray(new_vertices[ids[0]])
            struct.pack_into('<3f',center_data,0,*center)
            center_data[32:36]=bytes([palette.index(1 if hand==0 else 26),0,0,0])
            center_data[36:40]=bytes([255,0,0,0])
            middle=allocate(('center',hand),bytes(center_data))
            for a,b in zip(ids,ids[1:]+ids[:1]):
                # Native triangle winding faces away from the retained hand.
                u,v=sub(position(a),center),sub(position(b),center)
                normal=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                if dot(normal,planes[hand][1])>0:a,b=b,a
                clipped.append((a,b,middle));cap_count+=1
        stock.require(len(clipped)*3<=count,f'Insufficient original triangle capacity LOD{lod}: {len(clipped)} > {count//3}, removed={removed}, cap={cap_count}, allocated={len(new_vertices)}')
        values=[i for tri in clipped for i in tri]
        values.extend([values[0]]*(count-len(values)))
        struct.pack_into('<'+'H'*count,result,stock.IB_OFFSET+start*2,*values)
        stats.append(dict(lod=lod,removedTriangles=removed,capTriangles=cap_count,activeTriangles=len(clipped)))
    for index,data in new_vertices.items():result[stock.VB_OFFSET+index*40:stock.VB_OFFSET+(index+1)*40]=data
    stock.require(len(result)==len(body) and result[:stock.VB_OFFSET]==body[:stock.VB_OFFSET],
                  'Cooked layout/skeleton changed')
    for index in kept:
        stock.require(result[stock.VB_OFFSET+index*40:stock.VB_OFFSET+(index+1)*40]==vertices[index],
                      'Retained hand vertex changed')
    return bytes(result),dict(lods=stats,newVertices=len(new_vertices),retainedVertices=len(kept),
                              cuffMillimetres=5,textures='original unchanged',planes=planes,
                              capSkinning='submesh-palette',wristVertexIndices=[palette.index(1),palette.index(26)])
