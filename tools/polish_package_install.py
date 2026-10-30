"""Explicit-path, hash-gated install/rollback. Does not launch games or Steam."""
import argparse
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from polish_pack import require, sha

STEMS={'B.Font','H.Font','B.Texture_en','H.Texture_en','B.Misc_en','H.Misc_en','F.Misc_en','H.Misc'}
NAMES={stem+suffix for stem in STEMS for suffix in ('.cache','.toc')}

# Actual PSVR2 feedback: all menu glyphs became colored fragments after this
# atlas payload. Hash-valid extraction/preview was insufficient runtime proof.
REJECTED_FONT_PAYLOADS = {'B.Texture_en.cache': {
    '4754337fc9d60b4475ac4a47b397ce153679feefe4437fc81faf3687a331151c',
}}


def verify(package):
    manifest=json.loads((package/'manifest.json').read_text(encoding='utf8'))
    require(set(manifest['files'])==NAMES,'Unexpected package payload identities')
    require(not any(manifest['files'].get(name) in hashes for name,hashes in REJECTED_FONT_PAYLOADS.items()),
            'Font payload rejected by PSVR2 runtime; retain accepted font fallback')
    for name,expected in manifest['files'].items():
        require(sha(package/'payload'/name)==expected,'Package SHA256 mismatch: '+name)
    sums=(package/'SHA256SUMS').read_text(encoding='utf8').splitlines()
    seen=set()
    for row in sums:
        expected,name=row.split('  ',1);relative=Path(name)
        require(not relative.is_absolute() and '..' not in relative.parts and ':' not in name,'Unsafe checksum path')
        require(name not in seen,'Duplicate checksum path');seen.add(name)
        require((package/relative).resolve().is_relative_to(package.resolve()),'Checksum path escaped package')
        require(sha(package/relative)==expected,'Package checksum failed: '+name)
    require('manifest.json' in seen and all('payload/'+n in seen for n in NAMES),'Incomplete package checksums')
    return manifest


def game_free():
    if os.name!='nt':return
    output=subprocess.check_output(['tasklist','/FO','CSV','/NH'],text=True,errors='replace')
    names={r[0].casefold() for r in csv.reader(output.splitlines()) if r}
    require(not names.intersection({'darknessii.exe','thedarknessii.exe','darkness2.exe'}),'Close The Darkness II before installing/restoring')


def checked_copy(source,dest,expected):
    require(not dest.is_symlink(),'Refusing symlink destination')
    shutil.copyfile(source,dest)
    require(sha(dest)==expected,'Copied bytes mismatch: '+str(dest))


def install(package,cache,backup):
    manifest=verify(package);game_free()
    require(cache.is_dir(),'Cache directory missing')
    require(not backup.exists(),'Backup path already exists')
    actual={name:sha(cache/name) for name in NAMES}
    profiles=manifest['preimage_profiles']
    require(any(actual==profile for profile in profiles.values()),'Installed files are not an approved complete preimage; no files changed')
    require(all(not (cache/name).is_symlink() for name in NAMES),'Refusing linked cache files')
    backup.mkdir(parents=True)
    state=dict(schema=1,cache=str(cache.resolve()),original=actual,installed=manifest['files'],
               created_utc=datetime.now(timezone.utc).isoformat(),status='BACKUP_VERIFIED')
    for name in sorted(NAMES):checked_copy(cache/name,backup/name,actual[name])
    record=backup/'rollback.json';record.write_text(json.dumps(state,indent=2),encoding='utf8')
    changed=[]
    with tempfile.TemporaryDirectory(prefix='.darkness2-polish-',dir=cache) as folder:
        staging=Path(folder)
        for name in sorted(NAMES):checked_copy(package/'payload'/name,staging/name,manifest['files'][name])
        try:
            # Recheck after backup/staging in case another installer intervened.
            require(all(sha(cache/n)==actual[n] for n in NAMES),'Installed files changed during staging')
            for name in sorted(NAMES):
                os.replace(staging/name,cache/name);changed.append(name)
            require(all(sha(cache/n)==manifest['files'][n] for n in NAMES),'Installed verification failed')
            state['status']='INSTALLED_VERIFIED'
        except Exception:
            state['status']='INSTALL_FAILED_RESTORE_REQUIRED'
            record.write_text(json.dumps(state,indent=2),encoding='utf8')
            for name in reversed(changed):
                checked_copy(backup/name,staging/name,actual[name]);os.replace(staging/name,cache/name)
            require(all(sha(cache/n)==actual[n] for n in NAMES),'Immediate rollback verification failed; retain backup')
            state['status']='INSTALL_FAILED_ROLLBACK_VERIFIED'
            record.write_text(json.dumps(state,indent=2),encoding='utf8')
            raise
    record.write_text(json.dumps(state,indent=2),encoding='utf8')
    return state


def rollback(backup,cache):
    game_free();state=json.loads((backup/'rollback.json').read_text(encoding='utf8'))
    require(cache.resolve()==Path(state['cache']).resolve(),'Rollback target differs from backed-up cache')
    require(set(state['original'])==set(state['installed'])==NAMES,'Unexpected rollback identities')
    for name in NAMES:
        require(sha(backup/name)==state['original'][name],'Backup checksum mismatch: '+name)
        require(sha(cache/name) in (state['installed'][name],state['original'][name]),'Current file changed; refusing overwrite: '+name)
        require(not (cache/name).is_symlink(),'Refusing linked cache files')
    with tempfile.TemporaryDirectory(prefix='.darkness2-restore-',dir=cache) as folder:
        staging=Path(folder)
        for name in sorted(NAMES):checked_copy(backup/name,staging/name,state['original'][name])
        for name in sorted(NAMES):os.replace(staging/name,cache/name)
    require(all(sha(cache/n)==state['original'][n] for n in NAMES),'Restored hashes differ')
    state['status']='ROLLBACK_VERIFIED';(backup/'rollback.json').write_text(json.dumps(state,indent=2),encoding='utf8')
    return state


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='action',required=True)
    v=sub.add_parser('verify');v.add_argument('--package',type=Path,required=True)
    i=sub.add_parser('install');i.add_argument('--package',type=Path,required=True);i.add_argument('--cache',type=Path,required=True);i.add_argument('--backup',type=Path,required=True)
    r=sub.add_parser('rollback');r.add_argument('--cache',type=Path,required=True);r.add_argument('--backup',type=Path,required=True)
    a=parser.parse_args()
    if a.action=='verify':verify(a.package);print('PACKAGE_SHA256_PASS')
    elif a.action=='install':print(install(a.package,a.cache,a.backup)['status'])
    else:print(rollback(a.backup,a.cache)['status'])
