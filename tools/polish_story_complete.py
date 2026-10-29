"""Fit reviewed missing dialogue to exact native sample counts, streaming only."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import wave

from polish_pack import Cache, audio_format, chunks, command, patch_xwma, require, sha


def apply_patches(root,patches):
    """Append to the accepted audio baseline, preserving all accepted resources."""
    with tempfile.TemporaryDirectory(prefix='polish-audio-',dir=root.parent) as directory:
        for stem in ('H.Misc_en','B.Misc_en','F.Misc_en'):
            replacements={r['path']:base64.b64decode(r['payloads'][stem],validate=True) for r in patches}
            Cache(root,stem).write(directory,replacements)
        for file in Path(directory).iterdir():os.replace(file,root/file.name)


def verify_speech(stage,model_root,dependencies):
    for directory in reversed(dependencies):sys.path.insert(0,str(directory))
    import numpy as np
    from faster_whisper import WhisperModel
    import re
    model=WhisperModel(str(model_root),device='cpu',compute_type='int8',cpu_threads=2,local_files_only=True)
    patches=json.loads((stage/'reviewed-audio-patches.json').read_text(encoding='utf8'));report=[]
    for row in patches:
        file=stage/'reviewed-story'/(row['key']+'.decoded.wav')
        require(sha(file)==row['decoded_wav_sha256'],'Decoded QA source changed')
        with wave.open(str(file)) as w:
            rate=w.getframerate();a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype('float32')/32768
        a=np.interp(np.arange(round(len(a)*16000/rate))*rate/16000,np.arange(len(a)),a).astype('float32')
        segments,_=model.transcribe(a,language='pl',beam_size=5,vad_filter=True,condition_on_previous_text=False)
        text=' '.join(s.text.strip() for s in segments)
        expected=set(re.findall(r'\w+',row['text_pl'].casefold()));actual=set(re.findall(r'\w+',text.casefold()))
        coverage=len(expected&actual)/len(expected)
        missing=expected-actual
        # Proper names are reported for listening, never silently treated as
        # proof of correct pronunciation. All ordinary words must survive.
        names={'dżeki','queensboro'}
        passed=not (missing-names)
        report.append(dict(key=row['key'],expected=row['text_pl'],asr=text,word_coverage=coverage,
                           unmatched_expected_tokens=sorted(missing),proper_name_listening_pending=sorted(missing&names),
                           status='ASR_CONTENT_PASS_LISTENING_PENDING' if passed else 'FAIL_REVIEW_REQUIRED'))
    (stage/'reviewed-speech-qa.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    require(all(r['status']=='ASR_CONTENT_PASS_LISTENING_PENDING' for r in report),'Generated speech content regression; do not package')
    return report


def complete(stage,reviewed,donor,ffmpeg,encoder,dependencies):
    for directory in reversed(dependencies):sys.path.insert(0,str(directory))
    os.environ['HF_HUB_OFFLINE']='1'
    import numpy as np
    import torch
    from pocket_tts import TTSModel
    torch.set_num_threads(2);torch.manual_seed(0)
    lines=json.loads(reviewed.read_text(encoding='utf8'))
    require({r['key'] for r in lines}=={'Sd05150Vinnie','Sd05160Vinnie'},'Unreviewed story identities')
    model=TTSModel.load_model(config='hf://shefowl/pocket-tts-polish-6l/config.yaml',temp=0.75,quantize=True)
    state=model.get_state_for_audio_prompt(str(donor));folder=stage/'reviewed-story';folder.mkdir(exist_ok=True)
    patches=[];original_stream=Cache(stage/'baseline','F.Misc_en')
    for row in lines:
        key,path=row['key'],row['path'];original=stage/'missing-story'/(key+'.wav')
        require(sha(original)==row['source_wav_sha256'],'Reviewed source changed')
        spoken=row.get('text_tts',row['text_pl'])
        voice=folder/(key+'-'+hashlib.sha256((spoken+sha(donor)).encode()).hexdigest()[:12]+'.wav')
        if not voice.exists():
            data=model.generate_audio(state,spoken).detach().cpu().numpy().squeeze()
            with wave.open(str(voice),'wb') as w:
                w.setparams((1,2,model.sample_rate,0,'NONE','not compressed'));w.writeframes((np.clip(data,-1,1)*32767).astype('<i2').tobytes())
        h=(stage/'extract/H.Misc_en'/path).read_bytes();b=(stage/'extract/B.Misc_en'/path).read_bytes()
        require(path in original_stream.entries,'Resident replacement not supported')
        p,rate,ch,samples,avg,align,count,_=audio_format(h,b,original_stream.entries[path][3])
        with wave.open(str(voice)) as w:duration=w.getnframes()/w.getframerate()
        pcm=folder/(key+'.fit.wav');encoded=folder/(key+'.xwma');decoded=folder/(key+'.decoded.wav')
        frames=samples
        for attempt in range(8):
            tempo=max(1,duration/(frames/rate));filters=[]
            while tempo>2:filters.append('atempo=2');tempo/=2
            if tempo>1.000001:filters.append('atempo='+str(tempo))
            filters+=['apad','atrim=end_sample='+str(frames)]
            command([ffmpeg,'-v','error','-y','-i',voice,'-ar',rate,'-ac',ch,'-af','aresample='+str(rate)+','+','.join(filters),'-c:a','pcm_s16le',pcm])
            command([encoder,'-b',avg*8,pcm,encoded]);parts=chunks(encoded.read_bytes())
            actual=struct.unpack_from('<I',parts[b'dpds'],len(parts[b'dpds'])-4)[0]//(ch*2)
            if actual==samples:break
            frames-=actual-samples;require(frames>0,'Cannot fit codec sample count')
        require(actual==samples,'Cannot retain exact native timing; leave original installed')
        hh,bb,ff=patch_xwma(h,b,original_stream.entries[path][3],encoded.read_bytes())
        command([encoder,encoded,decoded])
        with wave.open(str(decoded)) as w:
            require((w.getframerate(),w.getnchannels(),w.getnframes())==(rate,ch,samples),'Decoded timing mismatch')
        patches.append(dict(**row,native_samples=samples,native_rate=rate,decoded_wav_sha256=sha(decoded),
                            generated_wav_sha256=sha(voice),encoder_sha256=sha(encoder),donor_sha256=sha(donor),
                            status='EXACT_NATIVE_SAMPLE_COUNT_DECODE_PASS_HEADSET_LISTENING_PENDING',
                            payloads={stem:base64.b64encode(data).decode() for stem,data in (('H.Misc_en',hh),('B.Misc_en',bb),('F.Misc_en',ff))}))
        print(key,'exact native samples',samples,flush=True)
    # Apply only after both replacements decode successfully.
    output=stage/'reviewed-audio-patches.json'
    require(not output.exists(),'Reviewed replacements already recorded; do not append twice')
    apply_patches(stage/'candidate',patches)
    output.write_text(json.dumps(patches,ensure_ascii=False,indent=2),encoding='utf8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','reviewed','donor','ffmpeg','encoder'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--dependency-dir',type=Path,action='append',default=[])
    a=parser.parse_args();complete(a.stage,a.reviewed,a.donor,a.ffmpeg,a.encoder,a.dependency_dir)
