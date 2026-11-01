"""MAIN-only deployment of existing private packs; retains verified preimages.

Payloads remain outside Git. Does not launch or drive the game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def closed():
    result = subprocess.check_output([
        'powershell', '-NoProfile', '-Command',
        "@(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object { $_.Name -in @('DarknessII.exe','TheDarknessII.exe','Darkness2.exe') }).Count"
    ], text=True).strip()
    require(result == '0', 'Close game before deployment or rollback')


def run(*args):
    subprocess.run([sys.executable, '-B', *map(str, args)], check=True)


def texture_readback(cache, backup, manifest, payload):
    # Check every replacement and forbid changes to unrelated TOC fields.
    for stem, tag in [('H.Texture', 'h'), ('B.Texture', 'b'), ('F.Texture', 'f')]:
        old = (backup / (stem + '.toc')).read_bytes()
        new = (cache / (stem + '.toc')).read_bytes()
        require(len(new) == len(old) and new[:8] == old[:8], 'TOC structure changed')
        dirs, entries = [''], {}
        for pos in range(8, len(new), 96):
            off, stamp, packed, size, padding, parent = struct.unpack_from('<QQIIII', new, pos)
            require(parent < len(dirs), 'Invalid parent')
            name = new[pos+32:pos+96].split(b'\0', 1)[0].decode('utf-8')
            path = (dirs[parent] + '/' + name).lstrip('/')
            if off == 0xffffffffffffffff:
                dirs.append(path)
            elif stamp:
                require(path not in entries, 'Duplicate entry')
                entries[path] = (pos, off, packed, size, padding)
        allowed, count = set(), 0
        with (cache / (stem + '.cache')).open('rb') as stream:
            for item in manifest['items']:
                if not item[tag + '_size']:
                    continue
                pos, off, packed, size, padding = entries[item['path']]
                require(packed == size == item[tag + '_size'] and padding == 0, 'Texture size/padding mismatch')
                stream.seek(off)
                require(hashlib.sha256(stream.read(size)).hexdigest() == item[tag + '_sha256'], 'Texture readback mismatch')
                allowed.update(range(pos, pos+8))
                allowed.update(range(pos+16, pos+28))
                count += 1
        require(all(a == b or i in allowed for i, (a, b) in enumerate(zip(old, new))), 'Unrelated TOC mutation')
        # Installer appends only; all pre-existing cache bytes must survive.
        with (backup / (stem + '.cache')).open('rb') as before, (cache / (stem + '.cache')).open('rb') as after:
            while chunk := before.read(4*1024*1024):
                require(after.read(len(chunk)) == chunk, 'Original texture cache prefix changed')
        print(stem, count, 'payloads verified; unrelated TOC and cache bytes preserved', flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    actions = ap.add_mutually_exclusive_group(required=True)
    actions.add_argument('--install', action='store_true')
    actions.add_argument('--rollback', action='store_true')
    ap.add_argument('--asset-repo', type=Path)
    ap.add_argument('--polish', type=Path)
    ap.add_argument('--game', type=Path)
    ap.add_argument('--backup', type=Path, required=True)
    a = ap.parse_args()
    closed()
    if a.rollback:
        record = json.loads((a.backup/'deployment.json').read_text(encoding='utf-8'))
        cache = Path(record['cache'])
        for n, h in record['before'].items():
            require(Path(n).name == n, 'Invalid rollback name')
            require(sha(a.backup/n) == h, 'Backup mismatch: ' + n)
            require(sha(cache/n) in (h, record.get('after', {}).get(n)), 'Current file changed: ' + n)
        for n in record['before']:
            shutil.copy2(a.backup/n, cache/n)
        require(all(sha(cache/n) == h for n,h in record['before'].items()), 'Rollback mismatch')
        record['status'] = 'ROLLBACK_VERIFIED'
        (a.backup/'deployment.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
        print('ROLLBACK_VERIFIED')
        return
    require(a.asset_repo and a.polish and a.game, 'Install requires asset-repo, polish and game paths')
    cache = a.game / 'Cache.Windows'
    textures = a.asset_repo / 'extracted_textures/package'
    installer = a.polish / 'tools/polish_package_install.py'
    manager = a.asset_repo / 'tools/manage_comic_textures.py'
    run(installer, 'verify', '--package', a.polish)
    pm = json.loads((a.polish / 'manifest.json').read_text(encoding='utf-8'))
    tm = json.loads((textures / 'manifest.json').read_text(encoding='utf-8'))
    require(tm['count'] == len(tm['items']) == 64, 'Expected 64 textures')
    require(len({i['path'] for i in tm['items']}) == 64, 'Duplicate textures')
    for item in tm['items']:
        rel = Path(item['path'])
        require(not rel.is_absolute() and '..' not in rel.parts and ':' not in str(rel), 'Unsafe texture path')
        for tag in ('h', 'b', 'f'):
            if item[tag + '_size']:
                file = textures / tag.upper() / rel
                require(file.stat().st_size == item[tag + '_size'] and sha(file) == item[tag + '_sha256'], 'Texture payload mismatch')
    names = sorted(set(pm['files']) | {s+e for s in ('H.Texture', 'B.Texture', 'F.Texture') for e in ('.toc', '.cache')})
    require(len(names) == 22, 'Unexpected target set')
    before = {n: sha(cache / n) for n in names}
    require(any({n: before[n] for n in pm['files']} == p for p in pm['preimage_profiles'].values()), 'Unapproved Polish preimage')
    for s in ('H.Texture', 'B.Texture', 'F.Texture'):
        for e in ('.toc', '.cache'):
            bak = cache / (s+e+'.orig_bak')
            require(not bak.exists() or sha(bak) == before[s+e], 'Texture manager would reset existing modifications')
    protected = [a.game / n for n in ('d3d9.dll', 'DarknessII.exe', 'Darkness2VR.ini', 'Darkness2VR-diagnostic.ini')]
    protected.append(Path('C:/Users/Trilex/AppData/Local/Darkness2VR/Darkness2VR.ini'))
    protected_hashes = {str(p): sha(p) for p in protected}
    require(not a.backup.exists(), 'Backup already exists')
    a.backup.mkdir(parents=True)
    record = dict(status='BACKING_UP', cache=str(cache), before=before, protected=protected_hashes,
                  polish_package=str(a.polish), texture_package=str(textures),
                  inputs={str(p): sha(p) for p in (installer, manager, a.polish/'manifest.json', textures/'manifest.json')})
    receipt = a.backup / 'deployment.json'
    receipt.write_text(json.dumps(record, indent=2), encoding='utf-8')
    for n in names:
        shutil.copy2(cache / n, a.backup / n)
        require(sha(a.backup / n) == before[n], 'Backup mismatch: ' + n)
    record['status'] = 'BACKUP_VERIFIED'
    receipt.write_text(json.dumps(record, indent=2), encoding='utf-8')
    print('All 22 preimage backups verified', flush=True)
    closed()
    try:
        require(all(sha(cache/n) == h for n,h in before.items()), 'Files changed during backup')
        run(installer, 'install', '--package', a.polish, '--cache', cache, '--backup', a.backup/'polish-transaction')
        run(manager, '--install', '--cache-dir', cache, '--package-dir', textures)
        require(all(sha(cache/n) == h for n,h in pm['files'].items()), 'Polish post-install mismatch')
        texture_readback(cache, a.backup, tm, textures)
        require(all(sha(Path(p)) == h for p,h in protected_hashes.items()), 'Protected runtime files changed')
        record.update(status='INSTALLED_VERIFIED', after={n: sha(cache/n) for n in names})
    except Exception:
        closed()
        for n in names:
            shutil.copy2(a.backup/n, cache/n)
        require(all(sha(cache/n) == h for n,h in before.items()), 'Rollback verification failed')
        record['status'] = 'FAILED_ROLLBACK_VERIFIED'
        receipt.write_text(json.dumps(record, indent=2), encoding='utf-8')
        raise
    receipt.write_text(json.dumps(record, indent=2), encoding='utf-8')
    print('INSTALLED_VERIFIED:', receipt, flush=True)


if __name__ == '__main__':
    main()
