"""Rebuild the exact release payload from private original/accepted inputs."""
import argparse
import json
from pathlib import Path
import shutil

from polish_pack import Cache, require, sha, translate
from polish_fonts import build_fonts


def rebuild(stage, baseline, extract, accepted_audio, translations, manifest):
    expected=json.loads(manifest.read_text(encoding='utf8'))
    require(not stage.exists(),'Rebuild stage must be new')
    require({p.name:sha(p) for p in baseline.iterdir() if p.name in expected['files']}==expected['preimage_profiles']['vanilla'],'Original input hashes differ')
    for stem in ('H.Misc_en','B.Misc_en','F.Misc_en'):
        for suffix in ('.cache','.toc'):
            name=stem+suffix;require(sha(accepted_audio/name)==expected['preimage_profiles']['accepted_polish'][name],'Accepted Polish audio input differs')
    stage.mkdir(parents=True);shutil.copytree(baseline,stage/'baseline');shutil.copytree(extract,stage/'extract')
    (stage/'candidate').mkdir()
    for name in expected['files']:
        source=accepted_audio/name if name.startswith(('H.Misc_en.','B.Misc_en.','F.Misc_en.')) else baseline/name
        shutil.copyfile(source,stage/'candidate'/name)
    build_fonts(stage)
    patches=translations.parent/'reviewed-audio-patches.json'
    if patches.exists():
        from polish_story_complete import apply_patches
        apply_patches(stage/'candidate',json.loads(patches.read_text(encoding='utf8')))
    data=next((stage/'extract/H.Misc').rglob('Languages.cl')).read_bytes()
    mapping=json.loads(translations.read_text(encoding='utf8'))
    for name,values in mapping.items():
        data,report=translate(data,('/D2/Language/'+name+'/').encode(),values)
        require(not report['retained_for_tokens'],'Rebuild translation token mismatch')
    cache=Cache(stage/'baseline','H.Misc');key=next(k for k in cache.entries if Path(k).name=='Languages.cl')
    cache.write(stage/'candidate',{key:data})
    actual={name:sha(stage/'candidate'/name) for name in expected['files']}
    require(actual==expected['files'],'Rebuilt payload differs from release')
    (stage/'rebuild-validation.json').write_text(json.dumps(dict(status='ALL_16_FILES_SHA256_IDENTICAL',files=actual),indent=2),encoding='utf8')
    print('ALL_16_FILES_SHA256_IDENTICAL')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','baseline','extract','accepted-audio','translations','manifest'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();rebuild(a.stage,a.baseline,a.extract,a.accepted_audio,a.translations,a.manifest)
