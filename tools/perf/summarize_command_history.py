"""Decode the bounded diagnostic ring; raw shader pointers are NOT shader hashes.

QPC ticks use the same clock as the existing ETW/PresentMon collector. This
reader does not start a capture, infer a pipeline compiler trigger, or alter VR.
"""
import argparse
import ctypes as c
import json
import struct
from pathlib import Path

class Pipeline(c.LittleEndianStructure):
    _fields_ = [(n, c.c_uint32) for n in ('vs','ps','declaration','fvf')] + [
        ('rt', c.c_uint32*4), ('depth',c.c_uint32), ('textures',c.c_uint32*8),
        ('render',c.c_uint32*16), ('sampler',c.c_uint32*6),
        ('known',c.c_uint32), ('renderKnown',c.c_uint32), ('samplerKnown',c.c_uint32)]

class Event(c.LittleEndianStructure):
    _fields_ = [(n,c.c_uint64) for n in ('sequence','begin','end')] + [
        (n,c.c_uint32) for n in ('frame','draw','tid','generation','device',
            'operation','eye','caller')] + [('args',c.c_uint32*6)] + [
        (n,c.c_uint32) for n in ('result','uncertain','resource')] + [('pipeline',Pipeline)]

OPS = ['unknown','draw','drawIndexed','drawUP','drawIndexedUP','createTexture',
       'createVS','createPS','producerFlush','blit','stateBlock','clear',
       'updateSurface','updateTexture','colorFill','diagnosticStateRead']

def plain(value):
    if isinstance(value,c.Array): return list(value)
    if isinstance(value,c.Structure): return {n:plain(getattr(value,n)) for n,*_ in value._fields_}
    return value

def read(path):
    data=Path(path).read_bytes()
    if len(data)<96: raise ValueError('Truncated history header')
    magic,version,size,count,freq,arm,trigger,written,overwritten,missing,snapshot,foreign=struct.unpack_from('<12Q',data)
    if magic!=0x3154534948323244 or version!=1: raise ValueError('Unsupported history format')
    if size!=c.sizeof(Event) or count>32768 or not freq: raise ValueError('Invalid history layout/count/clock')
    if len(data)!=96+size*count: raise ValueError('Truncated or trailing history bytes')
    events=[plain(Event.from_buffer_copy(data,96+i*size)) for i in range(count)]
    for i,e in enumerate(events):
        if i and e['sequence']!=events[i-1]['sequence']+1: raise ValueError('Noncontiguous history sequence')
        e['name']=OPS[e['operation']] if e['operation']<len(OPS) else 'unknown'
        e['wall_ms']=(e['end']-e['begin'])*1000/freq if e['end']>=e['begin'] and e['end'] else None
    return {'frequency':freq,'arm_qpc':arm,'trigger_sequence':trigger,'written':written,
            'overwritten':overwritten,'missing_ends':missing,'snapshot_overhead_ms':snapshot*1000/freq,
            'foreign_mutations':foreign,'events':events}

DRAWS={'draw','drawIndexed','drawUP','drawIndexedUP'}

def first_use_intervals(events):
    """Per producer flush: shaders first bound by a draw since the previous flush.

    "First" means first within the retained ring only; pointer identities are
    not hashes and may be reused after release. Intervals before the first
    retained flush are marked 'window_start' because earlier use is unknown.
    """
    seen_vs,seen_ps,seen_pair=set(),set(),set()
    rows=[];pending={'vs':0,'ps':0,'pairs':0,'draws':0}
    for e in events:
        if e['name'] in DRAWS and e['pipeline']['known']&3==3:
            vs,ps=e['pipeline']['vs'],e['pipeline']['ps']
            pending['draws']+=1
            if vs not in seen_vs: seen_vs.add(vs);pending['vs']+=1
            if ps not in seen_ps: seen_ps.add(ps);pending['ps']+=1
            if (vs,ps) not in seen_pair: seen_pair.add((vs,ps));pending['pairs']+=1
        elif e['name']=='producerFlush':
            rows.append({'sequence':e['sequence'],'frame':e['frame'],'eye':e['args'][0],
                'flush_ms':e['wall_ms'],'new_vs':pending['vs'],'new_ps':pending['ps'],
                'new_pairs':pending['pairs'],'draws':pending['draws'],'window_start':not rows})
            pending={'vs':0,'ps':0,'pairs':0,'draws':0}
    return rows

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('history',type=Path)
    parser.add_argument('--around-qpc',type=int)
    parser.add_argument('--before',type=int,default=32,choices=range(0,513))
    parser.add_argument('--first-use',action='store_true',help='per-flush first-bound shader counts')
    args=parser.parse_args();result=read(args.history);events=result.pop('events')
    if args.first_use:
        result['first_use']=first_use_intervals(events)
        result['limits']='First use within the retained ring only; pointer IDs are not hashes. Driver compilation still requires ETW worker evidence.'
        print(json.dumps(result,indent=2));return
    if args.around_qpc is not None:
        index=min(range(len(events)),key=lambda i:abs(events[i]['begin']-args.around_qpc)) if events else -1
    else: index=next((i for i,e in enumerate(events) if e['sequence']==result['trigger_sequence']),len(events)-1)
    result['context']=events[max(0,index-args.before):index+1]
    result['limits']='Sampled fields only; known masks and uncertain flag must be honored. Pointer reuse needs create events. Compiler association requires ETW worker dependency, not just temporal proximity.'
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
