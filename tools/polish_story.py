"""Decode and account for missing story assets using native metadata, offline.

ASR is a draft, never evidence that an unrecognized clip is nonverbal. Native
clips remain installed until an explicitly reviewed replacement is available.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import sys
import wave

from polish_pack import Cache, audio_format, command, require, sha


def prepare(stage, backlog, encoder, model_root, dependencies):
    for directory in reversed(dependencies):sys.path.insert(0,str(directory))
    from faster_whisper import WhisperModel
    model=WhisperModel(str(model_root),device='cpu',compute_type='int8',cpu_threads=2,local_files_only=True)
    rows=list(csv.DictReader(backlog.open(encoding='utf-8-sig')))
    rows=[r for r in rows if r['priority']=='1_SP_STORY' and r['status']=='MISSING_PL_RECORDING']
    require(len(rows)==38,'Missing story inventory changed; review identities')
    folder=stage/'missing-story';folder.mkdir(exist_ok=True)
    cache=Cache(stage/'baseline','F.Misc_en');report=[]
    for row in rows:
        path,key=row['native_path'],row['key'];wav=folder/(key+'.wav')
        h=(stage/'extract/H.Misc_en'/path).read_bytes()
        b=(stage/'extract/B.Misc_en'/path).read_bytes()
        resident=path not in cache.entries
        size=None if resident else cache.entries[path][3]
        p,rate,ch,samples,avg,align,count,annotation=audio_format(h,b,size,resident)
        if resident:
            size=struct.unpack_from('<I',h,p+30)[0];payload=b[-size:]
            dpds=b[len(b)-size-count*4:len(b)-size] if count else b''
        else:
            _,off,packed,size=cache.entries[path];require(packed==size,'Extract compressed stream first')
            with (stage/'baseline/F.Misc_en.cache').open('rb') as stream:stream.seek(off);payload=stream.read(size)
            dpds=b[annotation:]
        if not wav.exists():
            if count:
                fmt=struct.pack('<HHIIHHH',0x161,ch,rate,avg,align,16,0)
                chunks=[(b'fmt ',fmt),(b'dpds',dpds),(b'data',payload)]
                body=b'XWMA'+b''.join(tag+struct.pack('<I',len(v))+v+(b'\0' if len(v)&1 else b'') for tag,v in chunks)
                compressed=folder/(key+'.xwma');compressed.write_bytes(b'RIFF'+struct.pack('<I',len(body))+body)
                command([encoder,compressed,wav])
            else:
                require(len(payload)==samples*ch*2,'Original PCM bounds')
                with wave.open(str(wav),'wb') as stream:
                    stream.setparams((ch,2,rate,0,'NONE','not compressed'));stream.writeframes(payload)
        with wave.open(str(wav)) as stream:
            require(stream.getframerate()==rate and stream.getnchannels()==ch and stream.getnframes()==samples,'Original decode/sample contract')
        checkpoint=folder/(key+'.json')
        if checkpoint.exists():item=json.loads(checkpoint.read_text(encoding='utf8'))
        else:
            import numpy as np
            with wave.open(str(wav)) as stream:
                audio=np.frombuffer(stream.readframes(samples),dtype='<i2').astype(np.float32).reshape(-1,ch).mean(axis=1)/32768
            audio=np.interp(np.arange(round(len(audio)*16000/rate))*rate/16000,np.arange(len(audio)),audio).astype(np.float32)
            segments,info=model.transcribe(audio,language='en',beam_size=5,vad_filter=True,condition_on_previous_text=False)
            segments=list(segments)
            item=dict(key=key,path=path,native_samples=samples,native_rate=rate,native_channels=ch,
                      native_payload_sha256=hashlib.sha256(payload).hexdigest(),decoded_sha256=sha(wav),
                      transcript=' '.join(s.text.strip() for s in segments),
                      segments=[dict(text=s.text,start=s.start,end=s.end,avg_logprob=s.avg_logprob,no_speech_prob=s.no_speech_prob) for s in segments],
                      status='ORIGINAL_RETAINED_ASR_DRAFT_REQUIRES_REVIEW' if segments else 'ORIGINAL_RETAINED_NO_TRANSCRIPT_NOT_PROOF_OF_NONVERBAL')
            checkpoint.write_text(json.dumps(item,ensure_ascii=False,indent=2),encoding='utf8')
        report.append(item);print(key,item['transcript'],flush=True)
    (stage/'missing-story.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','backlog','encoder','model-root'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--dependency-dir',type=Path,action='append',default=[])
    args=parser.parse_args();prepare(args.stage,args.backlog,args.encoder,args.model_root,args.dependency_dir)
