"""MAIN only: verified fourteen-file asset repair install or exact rollback."""
import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile

from polish_pack import require, sha
import importlib.util

spec=importlib.util.spec_from_file_location('combined_deploy',Path(__file__).with_name('deploy-polish-comic.py'))
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)

STEMS={'H.Texture','B.Texture','F.Texture','H.Font','B.Font','H.Texture_en','B.Texture_en'}
NAMES={s+e for s in STEMS for e in ('.cache','.toc')}


def copy(source,dest,expected):
    require(not dest.is_symlink(),'Symlink destination')
    shutil.copy2(source,dest);require(sha(dest)==expected,'Copy mismatch: '+str(dest))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=('install','rollback'))
    ap.add_argument('--package',type=Path)
    ap.add_argument('--backup',type=Path,required=True)
    ap.add_argument('--game',type=Path,required=True)
    a=ap.parse_args();helper.closed();cache=a.game/'Cache.Windows'
    record=a.backup/'deployment.json'
    if a.action=='rollback':
        state=json.loads(record.read_text(encoding='utf-8'))
        require(state['cache']==str(cache.resolve()),'Rollback target differs')
        require(set(state['before'])==NAMES and set(state['after'])==NAMES,'Unexpected rollback set')
        for n,h in state['before'].items():
            require(sha(a.backup/n)==h,'Backup mismatch')
            require(sha(cache/n) in (h,state['after'][n]),'Current file changed: '+n)
        for n,h in state['before'].items():copy(a.backup/n,cache/n,h)
        state['status']='ROLLBACK_VERIFIED';record.write_text(json.dumps(state,indent=2),encoding='utf-8')
        print('ROLLBACK_VERIFIED');return
    require(a.package,'Package required')
    m=json.loads((a.package/'manifest.json').read_text(encoding='utf-8'))
    require(m['kind']=='texture-font-repair' and set(m['before'])==set(m['files'])==NAMES,'Unexpected package targets')
    require(all(sha(a.package/'payload'/n)==h for n,h in m['files'].items()),'Payload hash mismatch')
    require(all(sha(cache/n)==h for n,h in m['before'].items()),'Installed preimage differs')
    protected=[a.game/n for n in ('d3d9.dll','DarknessII.exe','Darkness2VR.ini','Darkness2VR-diagnostic.ini')]
    protected.append(Path('C:/Users/Trilex/AppData/Local/Darkness2VR/Darkness2VR.ini'))
    protected += [cache/(s+e) for s in ('H.Misc','B.Misc_en','H.Misc_en','F.Misc_en') for e in ('.cache','.toc')]
    hashes={str(p):sha(p) for p in protected}
    require(not a.backup.exists(),'Fresh backup path required');a.backup.mkdir(parents=True)
    state=dict(status='BACKING_UP',cache=str(cache.resolve()),before=m['before'],after=m['files'],
               protected=hashes,package=str(a.package.resolve()),manifest_sha256=sha(a.package/'manifest.json'))
    for n,h in m['before'].items():copy(cache/n,a.backup/n,h)
    state['status']='BACKUP_VERIFIED';record.write_text(json.dumps(state,indent=2),encoding='utf-8')
    print('14 backups verified',flush=True)
    with tempfile.TemporaryDirectory(prefix='.d2-asset-repair-',dir=cache) as folder:
        staging=Path(folder)
        for n,h in m['files'].items():copy(a.package/'payload'/n,staging/n,h)
        helper.closed()
        require(all(sha(cache/n)==h for n,h in m['before'].items()),'Preimage changed during staging')
        try:
            for n in sorted(NAMES):os.replace(staging/n,cache/n)
            require(all(sha(cache/n)==h for n,h in m['files'].items()),'Installed readback mismatch')
            require(all(sha(Path(p))==h for p,h in hashes.items()),'Protected runtime/audio/text changed')
            state['status']='INSTALLED_VERIFIED'
        except Exception:
            helper.closed()
            for n,h in m['before'].items():copy(a.backup/n,cache/n,h)
            state['status']='FAILED_ROLLBACK_VERIFIED'
            record.write_text(json.dumps(state,indent=2),encoding='utf-8');raise
    record.write_text(json.dumps(state,indent=2),encoding='utf-8')
    print('INSTALLED_VERIFIED',record,flush=True)


if __name__=='__main__':main()
