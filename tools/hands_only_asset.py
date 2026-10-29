"""Private, reversible stock FPJackieArms sleeve removal. No game data in Git.

Prepare from QuickBMS-extracted H/B FPJackieArms_skel.fbx. Install appends one
raw resource and atomically redirects its TOC entry; original cache bytes stay
intact. Rollback accepts only the recorded before/after states.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import struct

HEADER_SHA = '92080c173b952857f92ad275d776b53e58501e81b35a1bba03052901a8c85311'
BODY_SHA = 'cbbbdccd40daaa98c534c9790d2c681ea687ec007b1a9cf24356b720bc9fd679'
EXE_SHA = 'd46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1'
RESOURCE = '/D2/Characters/Camera/FPJackieArms_skel.fbx'
VB_OFFSET, VB_SIZE, IB_OFFSET, INDEX_COUNT = 5894, 114400, 120294, 42909
ENTRY_OFFSET, ENTRY_SIZE = 91129648, 176296
ENTRY_SHA = '70590e21d645601e7168cd31d56f54c14df6dea35ce56f97cc53186c231b44a6'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h


def require(ok, message):
    if not ok:
        raise ValueError(message)


def ranges(header, label):
    require(header.count(label) == 1, 'Ambiguous mesh part')
    at = header.index(label)
    require(struct.unpack_from('<I', header, at - 4)[0] == len(label),
            'Unexpected native string length')
    table = struct.unpack_from('<10I', header, at + len(label))
    return list(zip(table[:5], table[5:]))


def patch(header, body):
    require(sha(header) == HEADER_SHA and sha(body) == BODY_SHA, 'Stock asset preimage mismatch')
    require(len(body) == IB_OFFSET + INDEX_COUNT * 2, 'Unexpected mesh layout')
    require(b'/D2/Characters/Jackie/JackieFPArmsHandRevised' in header and
            b'/D2/Characters/Jackie/JackieFPArmsSleeveRevised' in header, 'Wrong materials')
    hands = ranges(header, b'/HandRevised1')
    sleeves = ranges(header, b'/SleeveRevised')
    covered = set()
    for start, count in hands + sleeves:
        require(start % 3 == 0 and count % 3 == 0 and start + count <= INDEX_COUNT,
                'Invalid triangle range')
        r = set(range(start, start + count))
        require(not covered.intersection(r), 'Overlapping material ranges')
        covered.update(r)
    require(len(covered) == INDEX_COUNT, 'Unknown mesh range')
    output = bytearray(body)
    # Retain the first index of each triangle. All three remain inside the
    # original draw's vertex range. Degenerate triangles rasterize no sleeve.
    for start, count in sleeves:
        for index in range(start, start + count, 3):
            at = IB_OFFSET + 2 * index
            output[at + 2:at + 6] = output[at:at + 2] * 2
    require(output[:IB_OFFSET] == body[:IB_OFFSET], 'Vertex/skeleton mutation')
    for start, count in hands:
        a, b = IB_OFFSET + start * 2, IB_OFFSET + (start + count) * 2
        require(output[a:b] == body[a:b], 'Hand geometry mutation')
    return bytes(output), hands, sleeves


def toc_entry(data):
    require(data[:4] == bytes.fromhex('4ec66718') and (len(data) - 8) % 96 == 0,
            'Unsupported TOC')
    dirs, found = [''], []
    for at in range(8, len(data), 96):
        offset, stamp, zsize, size, pad, parent, raw = struct.unpack_from('<QQIIII64s', data, at)
        require(parent < len(dirs), 'Invalid TOC parent')
        path = dirs[parent] + '/' + raw.split(b'\0')[0].decode('utf-8')
        if offset == 0xffffffffffffffff:
            dirs.append(path)
        elif stamp and path == RESOURCE:
            found.append((at, offset, zsize, size, pad))
    require(len(found) == 1, 'Resource missing or ambiguous')
    return found[0]


def game_closed():
    require(os.name == 'nt', 'Installation requires Windows process check')
    class Entry(ctypes.Structure):
        _fields_ = [('size', wintypes.DWORD), ('usage', wintypes.DWORD),
                    ('pid', wintypes.DWORD), ('heap', ctypes.c_size_t),
                    ('module', wintypes.DWORD), ('threads', wintypes.DWORD),
                    ('parent', wintypes.DWORD), ('priority', wintypes.LONG),
                    ('flags', wintypes.DWORD), ('exe', wintypes.WCHAR * 260)]
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(Entry)]
    kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(Entry)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateToolhelp32Snapshot(2, 0)
    require(handle != ctypes.c_void_p(-1).value, 'Cannot inspect game processes')
    try:
        entry = Entry(); entry.size = ctypes.sizeof(entry)
        require(kernel.Process32FirstW(handle, ctypes.byref(entry)), 'Process inventory unavailable')
        while True:
            require(entry.exe.lower() not in {'darknessii.exe', 'darkness2.exe', 'darkness.exe'},
                    'Close the game before asset changes')
            if not kernel.Process32NextW(handle, ctypes.byref(entry)):
                require(ctypes.get_last_error() == 18, 'Process inventory incomplete')
                break
    finally:
        kernel.CloseHandle(handle)


def atomic(path, data):
    stage = path.with_name(path.name + '.hands-only-stage')
    with open(stage, 'xb') as f:
        f.write(data); f.flush(); os.fsync(f.fileno())
    try:
        os.replace(stage, path)
    finally:
        stage.unlink(missing_ok=True)


def prepare(header_path, body_path, output):
    header, body = header_path.read_bytes(), body_path.read_bytes()
    result, hands, sleeves = patch(header, body)
    output.mkdir(parents=True, exist_ok=False)
    for name, data in [('original-header.bin', header), ('original-body.bin', body),
                       ('hands-only-body.bin', result)]:
        (output / name).write_bytes(data)
    receipt = dict(resource=RESOURCE, original=BODY_SHA, modified=sha(result),
                   handRanges=hands, sleeveRanges=sleeves, textures='unchanged',
                   status='PREPARED_NOT_RUNTIME_VALIDATED')
    (output / 'asset.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(json.dumps(receipt))


def install(cache_dir, package, backup):
    game_closed()
    require(file_hash(cache_dir.parent / 'DarknessII.exe').hexdigest() == EXE_SHA, 'EXE mismatch')
    original = (package / 'original-body.bin').read_bytes()
    result, _, _ = patch((package / 'original-header.bin').read_bytes(), original)
    require((package / 'hands-only-body.bin').read_bytes() == result, 'Prepared payload mismatch')
    cache, toc = cache_dir / 'B.Misc.cache', cache_dir / 'B.Misc.toc'
    before_toc = toc.read_bytes()
    at, offset, zsize, size, pad = toc_entry(before_toc)
    require((offset, zsize, size, pad) == (ENTRY_OFFSET, ENTRY_SIZE, len(original), 0),
            'Resource is already modified or unsupported')
    length = cache.stat().st_size
    require(offset + zsize <= length, 'Resource outside cache')
    with open(cache, 'rb') as f:
        f.seek(offset)
        require(sha(f.read(zsize)) == ENTRY_SHA, 'Compressed stock resource preimage mismatch')
    digest = file_hash(cache)
    before_cache_hash = digest.hexdigest(); digest.update(result)
    after_toc = bytearray(before_toc)
    struct.pack_into('<Q', after_toc, at, length)
    struct.pack_into('<III', after_toc, at + 16, len(result), len(result), 0)
    backup.mkdir(parents=True, exist_ok=False)
    (backup / 'B.Misc.toc.original').write_bytes(before_toc)
    (backup / 'hands-only-body.bin').write_bytes(result)
    receipt = dict(cache=str(cache.resolve()), toc=str(toc.resolve()), originalLength=length,
                   originalCacheSha=before_cache_hash, modifiedCacheSha=digest.hexdigest(),
                   originalTocSha=sha(before_toc), modifiedTocSha=sha(after_toc),
                   payloadSha=sha(result), payloadSize=len(result), resource=RESOURCE,
                   status='PREPARED')
    receipt_path = backup / 'receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print('Recovery receipt: ' + str(receipt_path), flush=True)
    game_closed()
    require(toc.read_bytes() == before_toc and cache.stat().st_size == length and
            file_hash(cache).hexdigest() == before_cache_hash, 'Concurrent cache mutation')
    # Append first: until TOC replacement, the native reader still uses exactly
    # the original resource. The PREPARED receipt can recover either state.
    with open(cache, 'r+b') as f:
        f.seek(0, 2); require(f.tell() == length, 'Cache length changed')
        f.write(result); f.flush(); os.fsync(f.fileno())
    require(file_hash(cache).hexdigest() == receipt['modifiedCacheSha'], 'Append verification failed')
    game_closed()
    require(toc.read_bytes() == before_toc, 'Concurrent TOC mutation')
    atomic(toc, after_toc)
    require(sha(toc.read_bytes()) == receipt['modifiedTocSha'], 'TOC verification failed')
    receipt['status'] = 'INSTALLED_NOT_RUNTIME_VALIDATED'
    atomic(receipt_path, json.dumps(receipt, indent=2).encode())
    print(str(receipt_path))


def rollback(receipt_path):
    game_closed()
    r = json.loads(receipt_path.read_text(encoding='utf-8'))
    require(r['resource'] == RESOURCE, 'Wrong receipt')
    cache, toc = Path(r['cache']), Path(r['toc'])
    require(cache.name == 'B.Misc.cache' and toc == cache.with_suffix('.toc'), 'Wrong cache paths')
    original_toc = (receipt_path.parent / 'B.Misc.toc.original').read_bytes()
    require(sha(original_toc) == r['originalTocSha'], 'Backup mismatch')
    current_toc = sha(toc.read_bytes()); current_cache = file_hash(cache).hexdigest()
    require(current_toc in {r['originalTocSha'], r['modifiedTocSha']}, 'TOC changed since install')
    if current_cache not in {r['originalCacheSha'], r['modifiedCacheSha']}:
        # Recover an interrupted/short append only if every original byte still
        # matches and the partial tail is exactly our intended payload prefix.
        payload = (receipt_path.parent / 'hands-only-body.bin').read_bytes()
        require(sha(payload) == r['payloadSha'], 'Payload backup mismatch')
        length = cache.stat().st_size
        require(r['originalLength'] < length < r['originalLength'] + len(payload) and
                current_toc == r['originalTocSha'], 'Cache changed since install')
        h = hashlib.sha256()
        with open(cache, 'rb') as f:
            remaining = r['originalLength']
            while remaining:
                block = f.read(min(remaining, 4*1024*1024))
                require(bool(block), 'Truncated original cache')
                h.update(block); remaining -= len(block)
            require(h.hexdigest() == r['originalCacheSha'] and
                    f.read() == payload[:length-r['originalLength']], 'Cache changed since install')
    require(current_toc != r['modifiedTocSha'] or current_cache == r['modifiedCacheSha'],
            'TOC refers to missing appended resource')
    game_closed()
    if current_toc != r['originalTocSha']:
        atomic(toc, original_toc)
    if current_cache != r['originalCacheSha']:
        with open(cache, 'r+b') as f:
            f.truncate(r['originalLength']); f.flush(); os.fsync(f.fileno())
    require(file_hash(cache).hexdigest() == r['originalCacheSha'], 'Cache restore mismatch')
    print('RESTORED_ALL_HASHES_MATCH')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare')
    for name in ['header', 'body', 'output']: p.add_argument('--' + name, type=Path, required=True)
    p = commands.add_parser('install')
    for name in ['cache-dir', 'package', 'backup']: p.add_argument('--' + name, type=Path, required=True)
    p = commands.add_parser('rollback'); p.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'prepare': prepare(args.header, args.body, args.output)
    elif args.command == 'install': install(args.cache_dir, args.package, args.backup)
    else: rollback(args.receipt)
