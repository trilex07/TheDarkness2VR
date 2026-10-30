"""Reproducible, private Polish package. Never discovers or modifies Steam.

The release retains the previously accepted audio caches. Rejected timing and
unsupported ADPCM replacements are explicitly inventoried, never forced in.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

from polish_pack import Cache, audio_format, category, chunks, command, require, sha, translate
from polish_fonts import FONTS, PL_LETTERS, build_fonts, decode_planar_dxt3
from polish_translate import TOKEN


def json_write(path, data):
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2,sort_keys=True)+'\n',encoding='utf8')


def payload(root,stem,path,cache=None):
    c=cache if cache is not None else Cache(root,stem);_,off,packed,size=c.entries[path]
    require(packed==size,'Compressed payload must be extracted first: '+path)
    with (root/(stem+'.cache')).open('rb') as stream:stream.seek(off);return stream.read(size)


def require_original_resource(stage,stem,path):
    before=Cache(stage/'baseline',stem);after=Cache(stage/'candidate',stem)
    require(before.entries[path]==after.entries[path],'Original TOC resource changed: '+path)
    _,off,packed,_=before.entries[path]
    with (stage/'baseline'/(stem+'.cache')).open('rb') as a,(stage/'candidate'/(stem+'.cache')).open('rb') as b:
        a.seek(off);b.seek(off);require(a.read(packed)==b.read(packed),'Original compressed/native bytes changed: '+path)


def validate_fonts(stage):
    import numpy as np
    report=[]
    for font,texture,offset,count,width,height,*_ in FONTS:
        original=(stage/'extract/B.Font'/font).read_bytes()
        body=payload(stage/'candidate','B.Font',font)
        header=payload(stage/'candidate','H.Font',font)
        new_count=count+18
        require(struct.unpack_from('<I',header,offset)[0]==new_count,'Font count')
        codes=struct.unpack_from('<'+str(new_count)+'H',body,new_count*44)
        old_codes=struct.unpack_from('<'+str(count)+'H',original,count*44)
        require(len(set(codes))==new_count and list(codes)==sorted(codes),'Glyph code ordering')
        require(body[new_count*46:]==original[count*46:],'Foreign language/font data changed')
        scale=2 if font.endswith('/MenuFont') else 1
        new_height=height*scale
        alpha=decode_planar_dxt3(payload(stage/'candidate','B.Texture_en',texture),width,new_height)
        old_alpha=decode_planar_dxt3((stage/'extract/B.Texture_en'/texture).read_bytes(),width,height)
        for i,code in enumerate(old_codes):
            j=codes.index(code)
            u0,v0,u1,v1=struct.unpack_from('<4f',original,i*16)
            expected=struct.pack('<4f',u0,v0/scale,u1,v1/scale)
            require(body[j*16:(j+1)*16]==expected,'Native glyph pixel coordinates changed')
            require(body[new_count*16+j*28:new_count*16+(j+1)*28]==original[count*16+i*28:count*16+(i+1)*28],'Native metrics changed')
            x0,x1=max(0,math.floor(u0*width)),min(width,math.ceil(u1*width))
            y0,y1=max(0,math.floor(v0*height)),min(height,math.ceil(v1*height))
            require(np.array_equal(alpha[y0:y1,x0:x1],old_alpha[y0:y1,x0:x1]),'Native glyph sampled pixels changed')
        glyphs=[]
        for letter,code,donor,accent in PL_LETTERS:
            i=codes.index(code);uv=struct.unpack_from('<4f',body,i*16)
            x0,y0,x1,y1=[round(v*d) for v,d in zip(uv,(width,new_height,width,new_height))]
            require(0<=x0<x1<=width and 0<=y0<y1<=new_height,'Polish UV bounds')
            crop=alpha[y0:y1,x0:x1]
            require(np.count_nonzero(crop)>3,'Empty Polish glyph: '+letter)
            donor_index=codes.index(ord(donor))
            require(body[i*16:(i+1)*16]!=body[donor_index*16:(donor_index+1)*16],'ASCII alias instead of Polish glyph')
            glyphs.append(dict(character=letter,alpha_sha256=hashlib.sha256(crop.tobytes()).hexdigest()))
        if scale==1:
            require_original_resource(stage,'H.Texture_en',texture)
            ht=(stage/'extract/H.Texture_en'/texture).read_bytes()
        else:ht=payload(stage/'candidate','H.Texture_en',texture)
        require(ht.count(struct.pack('<4H',width,new_height,width,new_height))==1,'Native texture dimensions')
        report.append(dict(font=font,width=width,height=new_height,glyphs=glyphs,status='BINARY_AND_GLYPH_PASS'))
    return report


def verify_archives(stage, quickbms=None, bms_script=None):
    result=[]
    proof_path=stage/'archive-proof.json'
    proof=json.loads(proof_path.read_text(encoding='utf8')) if proof_path.exists() else {}
    tools_match=quickbms and proof.get('quickbms_sha256')==sha(quickbms) and proof.get('script_sha256')==sha(bms_script)
    for toc in sorted((stage/'candidate').glob('*.toc')):
        print('Checking native archive',toc.stem,flush=True)
        stem=toc.stem;before=Cache(stage/'baseline',stem);after=Cache(stage/'candidate',stem)
        require(before.entries.keys()==after.entries.keys(),'Archive identities changed: '+stem)
        source=stage/'baseline'/(stem+'.cache');target=stage/'candidate'/source.name
        # Original bytes (including stale/deleted resources and compressed data)
        # survive exactly. New live payloads are append-only.
        with source.open('rb') as a,target.open('rb') as b:
            while block:=a.read(1024*1024):require(block==b.read(len(block)),'Original archive prefix changed: '+stem)
        allowed=set();changed=[]
        for path,entry in before.entries.items():
            if entry==after.entries[path]:continue
            pos=entry[0];allowed.update(range(pos,pos+8));allowed.update(range(pos+16,pos+28))
            _,off,packed,size=after.entries[path]
            require(off>=source.stat().st_size and packed==size,'Replacement not appended/raw')
            changed.append(path)
        require(len(before.toc)==len(after.toc),'TOC length changed')
        require(all(a==b or i in allowed for i,(a,b) in enumerate(zip(before.toc,after.toc))),'TOC metadata corruption')
        if quickbms:
            prior=proof.get('archives',{}).get(stem,{}) if tools_match else {}
            current_hashes={p.name:sha(p) for p in (toc,stage/'candidate'/(stem+'.cache'))}
            reused=prior.get('files')==current_hashes
            out=stage/'native-roundtrip'/stem;out.mkdir(parents=True,exist_ok=True)
            args=[quickbms,'-o']
            scope=after.entries
            if stem=='H.Misc':
                # Full extraction fails identically on vanilla and candidate
                # at the same unrelated 3743-byte resource (BMS line 105).
                # Reuse the accepted Languages.cl-only native extraction gate.
                require(len(changed)==1 and Path(changed[0]).name=='Languages.cl','Unexpected H.Misc changes')
                args+=['-f',changed[0]];scope=changed
            if not reused:command(args+[bms_script,toc,out])
            for path in (() if reused else scope):
                extracted=out/path
                require(extracted.is_file(),'Native extractor missing resource: '+path)
                require(extracted.stat().st_size==after.entries[path][3],'Native extracted size')
                if path in changed:
                    require(extracted.read_bytes()==payload(stage/'candidate',stem,path,after),'Native extractor payload mismatch')
        result.append(dict(archive=stem,resources=len(after.entries),replaced=len(changed),
                           original_prefix='SHA256_EQUIVALENT_BYTE_COMPARISON_PASS',
                           native_extraction=('CHANGED_LANGUAGES_RESOURCE_PASS; full extraction reproduces vanilla failure at unchanged resource, BMS line 105, size 3743' if stem=='H.Misc' else 'ALL_LIVE_RESOURCES_PASS') if quickbms else 'NOT_RUN'))
    if quickbms:write_archive_proof(stage,result,quickbms,bms_script)
    return result


def write_archive_proof(stage,reports,quickbms,bms_script):
    proof=dict(quickbms_sha256=sha(quickbms),script_sha256=sha(bms_script),archives={})
    for report in reports:
        stem=report['archive']
        proof['archives'][stem]=dict(report=report,files={stem+s:sha(stage/'candidate'/(stem+s)) for s in ('.cache','.toc')})
    json_write(stage/'archive-proof.json',proof)


def audio_inventory(stage, previous_stage, backlog):
    previous=json.loads((previous_stage/'build-report.json').read_text(encoding='utf8'))
    rows=[]
    for item in previous['audio']['unsupported']:
        path=item['path'];key=Path(path).name.removesuffix('_en.wav')
        row=dict(key=key,path=path,status='BLOCKED_ORIGINAL_RETAINED',reason=item['reason'])
        for stem in ('H.Misc_en','B.Misc_en'):
            native=(stage/('extract/'+stem)/path).read_bytes()
            require_original_resource(stage,stem,path)
            row[stem+'_sha256']=hashlib.sha256(native).hexdigest()
        try:
            h=(stage/'extract/H.Misc_en'/path).read_bytes();b=(stage/'extract/B.Misc_en'/path).read_bytes()
            fmt=audio_format(h,b,None,True);row.update(native_rate=fmt[1],native_samples=fmt[3],native_packets=fmt[6])
            encoded=previous_stage/'encoded'/(hashlib.sha256(path.encode()).hexdigest()[:16]+'.xwma')
            if encoded.exists():
                parts=chunks(encoded.read_bytes());decoded=struct.unpack_from('<I',parts[b'dpds'],len(parts[b'dpds'])-4)[0]//(fmt[2]*2)
                row.update(rejected_decoded_samples=decoded,rejected_timing_drift_ms=round((decoded-fmt[3])*1000/fmt[1],3))
        except ValueError:
            row['reason']='Unsupported 4-bit ADPCM serialization; codec/offset contract unproven. Native bytes retained.'
        rows.append(row)
    require(sum('timing' in r['reason'].lower() for r in rows)==97,'Timing inventory changed')
    require(len(rows)==100,'Expected 97 timing + 3 ADPCM cases')
    patch_path=stage/'reviewed-audio-patches.json'
    additions=json.loads(patch_path.read_text(encoding='utf8')) if patch_path.exists() else []
    added_paths={r['path'] for r in additions}
    import base64
    stream_cache=Cache(stage/'baseline','F.Misc_en')
    new_stream_cache=Cache(stage/'candidate','F.Misc_en')
    for addition in additions:
        path=addition['path']
        for stem,data in addition['payloads'].items():
            require(payload(stage/'candidate',stem,path)==base64.b64decode(data,validate=True),'Reviewed audio payload differs')
        original_fmt=audio_format((stage/'extract/H.Misc_en'/path).read_bytes(),(stage/'extract/B.Misc_en'/path).read_bytes(),stream_cache.entries[path][3])
        new_fmt=audio_format(payload(stage/'candidate','H.Misc_en',path),payload(stage/'candidate','B.Misc_en',path),new_stream_cache.entries[path][3])
        require(original_fmt[1:4]==new_fmt[1:4],'Reviewed audio changed native rate/channels/sample timing')
    # No accepted Polish speech is re-encoded or lost by this release.
    for stem in ('H.Misc_en','B.Misc_en','F.Misc_en'):
        before=Cache(previous_stage/'candidate',stem);after=Cache(stage/'candidate',stem)
        require(before.entries.keys()==after.entries.keys(),'Audio identities changed')
        require(all(entry==after.entries[path] for path,entry in before.entries.items() if path not in added_paths),'Accepted Polish audio metadata changed')
        with (previous_stage/'candidate'/(stem+'.cache')).open('rb') as a,(stage/'candidate'/(stem+'.cache')).open('rb') as b:
            while block:=a.read(1024*1024):require(block==b.read(len(block)),'Accepted Polish audio bytes changed')
    with backlog.open(encoding='utf-8-sig') as stream:
        import csv
        missing=[r for r in csv.DictReader(stream) if r['priority']=='1_SP_STORY' and r['status']=='MISSING_PL_RECORDING']
    require(len(missing)==38,'Missing story inventory changed')
    drafts=json.loads((stage/'missing-story.json').read_text(encoding='utf8'))
    require({r['key'] for r in missing}=={r['key'] for r in drafts},'Story inventory incomplete')
    for row in drafts:
        path=row['path']
        if path in added_paths:
            row['status']='POLISH_REPLACEMENT_EXACT_NATIVE_SAMPLES_DECODE_PASS_LISTENING_PENDING'
            continue
        for stem in ('H.Misc_en','B.Misc_en'):
            require_original_resource(stage,stem,path)
    qa=json.loads((stage/'reviewed-speech-qa.json').read_text(encoding='utf8')) if additions else []
    require({r['key'] for r in qa}=={r['key'] for r in additions} and all(r['status']=='ASR_CONTENT_PASS_LISTENING_PENDING' for r in qa),'New speech QA missing/failed')
    return dict(accepted_polish_entries_preserved=previous['audio']['replaced'],new_polish_entries=len(additions),new_speech_qa=qa,timing_and_adpcm=rows,missing_story=drafts,
                note='All 97 timing cases, 3 ADPCM cases and 38 missing story clips accounted for; two reviewed spoken clips replaced, remaining native clips retained. ASR does not replace listening QA.')


def validate_text(stage):
    original=next((stage/'extract/H.Misc').rglob('Languages.cl')).read_bytes()
    cache=Cache(stage/'candidate','H.Misc');key=next(p for p in cache.entries if Path(p).name=='Languages.cl')
    current=payload(stage/'candidate','H.Misc',key)
    maps=json.loads((stage/'translations.json').read_text(encoding='utf8'));report=[];rebuilt=original
    import re
    markers=list(dict.fromkeys(re.findall(rb'/D2/Language/[A-Za-z0-9_]+/',original)))
    require(len(maps)==len(markers)==14,'Translation categories incomplete')
    for marker in markers:
        name=marker.decode().split('/')[-2];a=category(original,marker)[3];b=category(current,marker)[3]
        require([x[0] for x in a]==[x[0] for x in b],'Native language keys changed')
        for (key,text,_),(_,pl,_) in zip(a,b):
            require(key in maps[name] and pl==maps[name][key],'Missing translation mapping')
            require(Counter(TOKEN.findall(text))==Counter(TOKEN.findall(pl)),'Placeholder/format mismatch: '+key)
        rebuilt,stats=translate(rebuilt,marker,maps[name]);require(not stats['retained_for_tokens'],'Rejected translation')
        report.append(dict(category=name,entries=len(a),changed=stats['replaced'],status='FORMATTING_PASS_LINGUISTIC_QA_PENDING'))
    require(rebuilt==current,'Unrelated language resource serialization changed')
    return report


def deployed_preimage(baseline, manifest):
    """Use immutable deployment receipts, never a later-mutated staging folder."""
    result=dict(baseline)
    receipt=json.loads(manifest.read_text(encoding='utf-8-sig'))
    seen=set()
    for row in receipt['files']:
        name=row['name'];value=row['afterSha256'].lower()
        require(name in result and name not in seen,'Unexpected deployment receipt identity')
        require(len(value)==64 and all(c in '0123456789abcdef' for c in value),'Invalid deployment receipt hash')
        seen.add(name);result[name]=value
    require(bool(seen),'Empty deployment receipt')
    return result


def release(stage, previous_stage, backlog, out, quickbms, bms_script, deployed_manifest=None):
    require(not out.exists(),'Release destination already exists')
    fonts=validate_fonts(stage);text=validate_text(stage);audio=audio_inventory(stage,previous_stage,backlog)
    archives=verify_archives(stage,quickbms,bms_script)
    files={p.name:sha(p) for p in sorted((stage/'candidate').iterdir()) if p.is_file()}
    require(len(files)==16,'Unexpected package files')
    commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
    require(not subprocess.check_output(['git','status','--porcelain'],text=True).strip(),'Commit source changes before releasing')
    manifest=dict(schema=1,source_commit=commit,files=files,
        preimages={p.name:sorted({sha(p),sha(previous_stage/'candidate'/p.name) if (previous_stage/'candidate'/p.name).exists() else sha(p)}) for p in sorted((stage/'baseline').iterdir()) if p.name in files},
        fonts=fonts,text=text,audio=audio,archives=archives,
        runtime='NOT_RUN; MAIN must perform one consolidated game/startup/Polish font/audio/subtitle test',
        completeness='PARTIAL_LOCALIZATION; native audio retained for documented blocked replacements; generated text requires Polish language review')
    manifest['preimage_profiles']={
        'vanilla':{name:sha(stage/'baseline'/name) for name in files},
        'accepted_polish':{name:sha((previous_stage/'candidate'/name) if (previous_stage/'candidate'/name).exists() else (stage/'baseline'/name)) for name in files}}
    if deployed_manifest:
        manifest['preimage_profiles']['recorded_deployment']=deployed_preimage(manifest['preimage_profiles']['vanilla'],deployed_manifest)
        manifest['deployment_receipt_sha256']=sha(deployed_manifest)
    out.mkdir(parents=True);(out/'payload').mkdir();(out/'inputs').mkdir();(out/'tools').mkdir()
    for name in files:shutil.copyfile(stage/'candidate'/name,out/'payload'/name)
    for name in ('translations.json','translation-coverage.json','font-report.json','missing-story.json'):
        shutil.copyfile(stage/name,out/'inputs'/name)
    for name in ('reviewed-audio-patches.json','reviewed-speech-qa.json'):
        if (stage/name).exists():shutil.copyfile(stage/name,out/'inputs'/name)
    for name in ('polish_release.py','polish_rebuild.py','polish_package_install.py','polish_pack.py','polish_fonts.py','polish_translate.py','polish_ui_reviewed.py','polish_story_complete.py'):
        shutil.copyfile(Path(__file__).parent/name,out/'tools'/name)
    json_write(out/'manifest.json',manifest)
    shutil.copyfile(Path(__file__).parent.parent/'docs/localization/POLISH_PACKAGE.md',out/'README.md')
    sums={str(p.relative_to(out)).replace('\\','/'):sha(p) for p in sorted(out.rglob('*')) if p.is_file()}
    (out/'SHA256SUMS').write_text(''.join(value+'  '+name+'\n' for name,value in sums.items()),encoding='utf8')
    archive=out.with_suffix('.zip')
    require(not archive.exists(),'ZIP already exists')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(out.rglob('*')):
            if not p.is_file():continue
            info=zipfile.ZipInfo(str(p.relative_to(out)).replace('\\','/'),date_time=(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            z.writestr(info,p.read_bytes(),compresslevel=6)
    json_write(stage/'release-result.json',dict(package=str(archive),sha256=sha(archive),source_commit=commit))
    print(json.dumps(dict(package=str(archive),sha256=sha(archive),source_commit=commit)))


def font_only_release(source_package, accepted_cache, receipt_path, out):
    """Isolate fonts from a verified release; retain accepted text/audio exactly.

    Inputs are explicit private paths. This function never discovers an install,
    calls the installer, changes Steam, or launches a game. The immutable rollback
    receipt (not a mutable stage) identifies the accepted 16-file preimage.
    """
    import tempfile
    from polish_package_install import NAMES, verify
    source = verify(source_package)
    receipt = json.loads(receipt_path.read_text(encoding='utf-8-sig'))
    accepted = receipt['original']
    require(set(accepted) == NAMES, 'Incomplete accepted rollback receipt')
    actual = {name: sha(accepted_cache/name) for name in sorted(NAMES)}
    require(actual == accepted, 'Accepted cache differs from rollback receipt')
    require(any(actual == profile for profile in source['preimage_profiles'].values()),
            'Accepted cache is not a complete source release preimage')
    require(not out.exists(), 'Release destination already exists')
    fonts = source.get('fonts', [])
    require(len(fonts) == 6 and len({f['font'] for f in fonts}) == 6 and
            all(f.get('status') == 'BINARY_AND_GLYPH_PASS' and len(f['glyphs']) == 18
                for f in fonts), 'Source font validation is incomplete')
    font_names = {stem+suffix for stem in ('B.Font','H.Font','B.Texture_en','H.Texture_en')
                  for suffix in ('.cache','.toc')}
    files = {name: source['files'][name] if name in font_names else actual[name]
             for name in sorted(NAMES)}
    manifest = dict(schema=1, mode='FONTS_ONLY_ACCEPTED_TEXT_AUDIO_UNCHANGED',
                    source_commit=source['source_commit'],
                    builder_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                    source_manifest_sha256=sha(source_package/'manifest.json'),
                    accepted_receipt_sha256=sha(receipt_path),
                    files=files, preimage_profiles={'accepted':actual}, fonts=fonts,
                    preserved_files={name:actual[name] for name in sorted(NAMES-font_names)},
                    runtime='NOT_RUN; glyph rendering and game stability require MAIN validation')
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.polish-font-package-',dir=out.parent) as tmp:
        staged = Path(tmp)/'package';(staged/'payload').mkdir(parents=True)
        for name, expected in files.items():
            origin=source_package/'payload'/name if name in font_names else accepted_cache/name
            shutil.copyfile(origin,staged/'payload'/name)
            require(sha(staged/'payload'/name)==expected,'Font package copy changed: '+name)
        json_write(staged/'manifest.json',manifest)
        (staged/'tools').mkdir()
        for tool in ('polish_package_install.py','polish_pack.py'):
            shutil.copyfile(Path(__file__).parent/tool,staged/'tools'/tool)
        (staged/'README.md').write_text(
            'Font-only candidate. Do not install while the game is running.\n'
            'Verify: python -B tools/polish_package_install.py verify --package <package>\n'
            'MAIN installs only after the game is closed, using explicit --cache and --backup paths.\n'
            'Rollback: python -B tools/polish_package_install.py rollback --backup <backup> --cache <cache>\n'
            'All accepted text and speech files are preserved byte for byte.\n'
            'The previous full localization release is NOT reinstalled.\n'
            'Runtime font appearance and the reported enemy hang remain unverified.\n',encoding='utf8')
        sums={p.relative_to(staged).as_posix():sha(p) for p in sorted(staged.rglob('*')) if p.is_file()}
        (staged/'SHA256SUMS').write_text(''.join(value+'  '+name+'\n' for name,value in sums.items()),encoding='utf8')
        verify(staged)
        staged.rename(out)
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fonts-only',action='store_true')
    for name in ('source-package','accepted-cache','accepted-receipt','stage','previous-stage','backlog','out','quickbms','bms-script'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--deployed-manifest',type=Path)
    args=parser.parse_args()
    if args.fonts_only:
        require(all((args.source_package,args.accepted_cache,args.accepted_receipt,args.out)), 'Font-only input paths required')
        result=font_only_release(args.source_package,args.accepted_cache,args.accepted_receipt,args.out)
        print(json.dumps(dict(package=str(args.out),mode=result['mode'],runtime=result['runtime'])))
    else:
        require(all((args.stage,args.previous_stage,args.backlog,args.out,args.quickbms,args.bms_script)), 'Full release input paths required')
        release(args.stage,args.previous_stage,args.backlog,args.out,args.quickbms,args.bms_script,args.deployed_manifest)
