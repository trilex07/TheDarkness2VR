"""ONE existing 639c CLI call after the agent actually views current Main.

Default inert. No acquire, launch, focus, input, capture, retry or image TTL.
Use only inside a NEW explicit runtime grant; existing helper is unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def invoke(root, packet, *, backend, api, phase_type, owner_guard, call=subprocess.run):
    owned = json.loads((root/'session/owned-window.json').read_text())
    expected = json.loads((root/'bootstrap/focus-expected.json').read_text())
    expected['deadline'] -= 10  # Original A+240 -> admitted A+230, not a new TTL.
    if any(expected[k] != owned[k] for k in ('pid','hwnd','created_ticks','exe','token')):
        raise ValueError('Current bootstrap/owned-window identities differ')
    def check():
        if any((root/'session'/n).exists() for n in ('REQUEST-CLOSE','STOP-INPUT.json','NAVIGATION-FATAL.json')):
            raise ValueError('Lifecycle stopped')
        owner_guard(expected, backend, api)  # Current exact token/PID/creation/path/HWND/deadline.
        if api.foreground() != owned['hwnd']:
            raise ValueError('Exact native foreground differs')
    marker = '[Simulator] process-only runtime override='+str(Path(packet['runtime_json']))+' data='+owned['runtime']
    phase = phase_type(owned['log'], marker, check)
    check()
    if phase.read()['flags'] != 1:
        raise ValueError('CURRENT native phase is not Main')
    helper = Path(packet['native_helper'])
    if hashlib.sha256(helper.read_bytes()).hexdigest().upper() != '639C0E8AE64107A531408ACAB49441A96DB4D3FE4E8AE06E1FF7D2F7F603C822':
        raise ValueError('Unchanged639c helper differs')
    # Its exclusive output and two equal bounded reads remain the entire native protocol.
    args = [sys.executable,'-B',str(helper),'--exe',owned['exe'],'--pid',str(owned['pid']),
            '--hwnd',str(owned['hwnd']),'--created-ticks',str(owned['created_ticks']),
            '--session-id',expected['session_id'],'--owner-token',owned['token'],
            '--output',str(root/'native-once.json')]
    check()
    if phase.read()['flags'] != 1:
        raise ValueError('Main changed before the ONE invocation')
    result = call(args, check=False)  # Exactly once, including REFUSED. No retry.
    check()
    return result.returncode


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--live',action='store_true')
    p.add_argument('--session',type=Path)
    p.add_argument('--packet',type=Path)
    a = p.parse_args(argv)
    if not a.live:
        print('INERT: no files/native/lease/game action'); return 0
    if not a.session or not a.packet:
        p.error('--live requires CURRENT --session and reviewed --packet')
    raw = a.packet.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != '9A962FF06603E335D7140528AA4A91204A02CB266046782D5C667C7508757E1A':
        raise ValueError('Reviewed9A962 packet differs')
    packet = json.loads(raw)
    sys.path.insert(0,packet['tools'])
    from darkness_runtime_adapter import WindowsBackend
    from darkness_focus_only import NativeAPI, guard
    from darkness_slot_gate import CurrentPhase
    return invoke(a.session,packet,backend=WindowsBackend(),api=NativeAPI(),
                  phase_type=CurrentPhase,owner_guard=guard)


if __name__=='__main__':
    raise SystemExit(main())
