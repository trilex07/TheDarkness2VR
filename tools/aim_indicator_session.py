"""Thin current-profile binding of the existing lifecycle; default inert.

No acquisition, focus, capture, input or native read. No shared source edits.
--preflight reads files only. --run requires a NEW explicit grant and own lease.
"""
import argparse
from contextlib import contextmanager
import configparser
from datetime import datetime
import hashlib
import importlib
import json
from pathlib import Path
import sys
import time


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(262144), b''):
            h.update(block)
    return h.hexdigest().upper()


def load(path, expected):
    if digest(path) != expected.upper():
        raise ValueError('Reviewed packet SHA differs')
    packet = json.loads(Path(path).read_text())
    for item in packet['pins']:
        if digest(item['path']) != item['sha256'].upper():
            raise ValueError('Pinned file differs: ' + item['path'])
    if set(packet['profile']) != {'d3d9.dll', 'Darkness2VR.ini', 'Darkness2VR-diagnostic.ini'}:
        raise ValueError('Exact current profile required')
    for name, expected in packet['profile'].items():
        if digest(Path(packet['game']) / name) != expected:
            raise ValueError('Current profile changed; review, do not refresh: ' + name)
    for name, expected in packet['protected_files'].items():
        if digest(Path(packet['game']) / name) != expected:
            raise ValueError('Protected file differs: ' + name)
    if digest(packet['candidate']) != packet['candidate_sha256']:
        raise ValueError('Dot candidate differs')
    descriptor = json.loads(Path(packet['runtime_json']).read_text())
    if Path(descriptor['runtime']['library_path']).resolve() != Path(packet['simulator']).resolve():
        raise ValueError('Process-local runtime target differs')
    ini = configparser.ConfigParser(strict=False)
    ini.read(Path(packet['game']) / 'Darkness2VR.ini', encoding='utf-8-sig')
    if any(ini.get('Input', k, fallback='') != '1' for k in
           ('DoubleShakeReload', 'DeferredGripRetry', 'ThrowSolverFailElevation')):
        raise ValueError('User option1 values changed; review before a new session')
    return packet


def adapter_module(packet):
    sys.path.insert(0, packet['tools'])
    module = importlib.import_module('darkness_runtime_adapter')
    for name, item in (('darkness_runtime_adapter', module),
                       ('darkness_session_backup', module.backup)):
        if Path(item.__file__).resolve() != (Path(packet['tools']) / (name+'.py')).resolve():
            raise ValueError('Wrong imported dependency: ' + name)
    return module


@contextmanager
def binding(module, packet):
    old_candidate, old_profile = module.BFE, module.backup.PROFILE_HASHES
    try:
        module.BFE = packet['candidate_sha256']
        module.backup.PROFILE_HASHES = dict(packet['profile'])
        yield
    finally:
        module.BFE, module.backup.PROFILE_HASHES = old_candidate, old_profile


def run(packet, grant, root, module, *, now=time.time, backend=None):
    # Same original-acquisition freshness as prior lifecycle entrypoints.
    acquired = datetime.fromisoformat(grant['acquired_utc'])
    if acquired.tzinfo is None:
        raise ValueError('Timezone-aware original acquisition required')
    a = acquired.timestamp()
    if (grant.get('parent_granted_runtime') is not True
            or not grant.get('token') or not a <= now() < a+60):
        raise ValueError('Fresh explicit grant/original acquisition required')
    root = module.backup.check_path(root)
    if root.exists():
        raise ValueError('New unused session root required')
    class CurrentProfileAdapter(module.Adapter):
        MAX_INPUT_SECONDS = 360
        MAX_TOTAL_SECONDS = 480
    with binding(module, packet):
        adapter = CurrentProfileAdapter(**{k: packet[k] for k in
            ('game', 'local', 'roaming', 'candidate', 'settings', 'runtime_json', 'native_helper')},
            evidence=root/'session', token=grant['token'], stop_input=a+350,
            cleanup=a+480, backend=backend if backend is not None else module.WindowsBackend())
        adapter.lease(no_games=True)  # Exact live token, acquisition bounds and no games.
        root.mkdir()
        (root/'binding.json').write_text(json.dumps(dict(source=packet['source_commit'],
            candidate_sha256=packet['candidate_sha256'], profile=packet['profile'],
            token=grant['token'], acquired_utc=grant['acquired_utc'],
            stop_input=a+350, parent_input_max=a+360, cleanup=a+480), indent=2))
        print(json.dumps(dict(token=grant['token'], acquired_utc=grant['acquired_utc'],
                              stop_input=a+350, cleanup=a+480)), flush=True)
        adapter.run()  # Existing fresh backup/install/normal close/restore/release.


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--preflight', action='store_true')
    mode.add_argument('--run', action='store_true')
    for name in ('packet', 'packet-sha256', 'grant', 'root'):
        p.add_argument('--'+name)
    args = p.parse_args(argv)
    if not (args.preflight or args.run):
        print('INERT: no file reads, lease, native API or game action')
        return 0
    if not args.packet or not args.packet_sha256:
        p.error('Reviewed --packet and --packet-sha256 required')
    if args.run and (not args.grant or not args.root):
        p.error('--run requires NEW --grant receipt and unused --root')
    packet = load(args.packet, args.packet_sha256)
    module = adapter_module(packet)
    with binding(module, packet):
        module.backup.validate_recipe(Path(packet['game']))
        inventory = module.backup.inventory(*(Path(packet[k]) for k in ('game','local','roaming')))
    if args.preflight:
        print(json.dumps(dict(status='CPU_PREFLIGHT_PASS', pins=len(packet['pins']),
                              inventory=inventory, runtime_authorized=False), indent=2))
        return 0
    run(packet, json.loads(Path(args.grant).read_text()), Path(args.root), module)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
