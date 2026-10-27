"""Bounded eligibility audit of the existing five-argument text dump; no capture.
All observations are attempted calls, not successful device-state reconstruction.
The dump lacks DIP primitive count, bit-exact arena, initial state and query events.
Consequently this format cannot admit a draw merge, even for matching signatures.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re

COMMAND = re.compile(r'^(\d+) op=(\d+) a=([0-9,]+)$')
DRAWS = {13, 14, 15, 16}
# Only fully inline scalar arguments; no payload offsets treated as values.
SCALAR = {8: (1, 2), 9: (1, 2), 10: (2, 3), 11: (2, 3),
          17: (0, 1), 18: (0, 1), 19: (0, 1), 23: (1, 4),
          24: (1, 2), 25: (0, 1), 26: (0, 1)}
BARRIERS = {0, 1, 2, 3, 4, 5, 30, 31, 32, 33, 35, 36}


def audit(text):
    state = {}
    draws = []
    counts = collections.Counter()
    repeat = collections.Counter()
    pairs = collections.Counter()
    pair_records = []
    barrier = True
    epoch = 0
    last_index = -1
    frequencies = []
    for line in text.splitlines():
        m = COMMAND.fullmatch(line)
        if not m:
            if re.match(r'^\d+ op=', line):
                raise ValueError('Malformed command')
            continue  # annotations are deliberately not bit-exact state
        index, op = int(m[1]), int(m[2])
        a = tuple(map(int, m[3].split(',')))
        if index != last_index + 1 or len(a) != 5 or not 0 <= op <= 37:
            raise ValueError('Non-contiguous/unsupported five-argument trace')
        last_index = index
        counts[op] += 1
        if op in BARRIERS:
            barrier = True
        if op == 36:
            state.clear()  # initial/restored state is unknown
        if op in {6, 7, 12, 20, 21, 22, 27, 28, 29, 34, 36, 37}:
            epoch += 1  # opaque upload/state boundary, never compare arena offsets
        if op in SCALAR:
            key_count, used = SCALAR[op]
            key, value = (op, a[:key_count]), a[key_count:used]
            if key in state and state[key] == value:
                repeat[op] += 1
            # FVF and declaration interact; do not preserve stale counterpart.
            if op == 17:
                state.pop((18, ()), None)
            if op == 18:
                state.pop((17, ()), None)
            state[key] = value
        if op == 24:
            frequencies.append({'record': index, 'stream': a[0], 'setting': a[1]})
        if op not in DRAWS:
            continue
        stream = state.get((23, (0,)))
        ib = state.get((25, ()))
        vs, ps = state.get((19, ())), state.get((26, ()))
        decl = state.get((17, ())) or state.get((18, ()))
        geometry_known = bool(stream and stream[0] and decl and
                              (op != 14 or (ib and ib[0]))) and op in {13, 14}
        geometry = (op, a, tuple(sorted((k, v) for k, v in state.items()
                        if k[0] in {17, 18, 23, 24, 25})))
        material = tuple(sorted((k, v) for k, v in state.items()
                              if k[0] in {9, 10, 11, 19, 26}))
        freq0 = state.get((24, (0,)), (0,))[0]
        inst_streams = [k[1][0] for k, v in state.items()
                        if k[0] == 24 and v[0] & 0x80000000]
        instanced = op == 14 and bool(freq0 & 0x40000000) and bool(inst_streams)
        d = {'record': index, 'op': op, 'visible_args': a,
             'geometry_binding_known': geometry_known, 'geometry': geometry,
             'material': material, 'shaders_known': bool(vs and vs[0] and ps and ps[0]),
             'opaque_epoch': epoch, 'already_instanced': instanced,
             'instance_count': freq0 & 0x3fffffff if instanced else None,
             'vs_pointer': vs[0] if vs else None, 'ps_pointer': ps[0] if ps else None,
             'admission': 'BLOCKED_INCOMPLETE_CAPTURE'}
        if draws:
            prev = draws[-1]
            if barrier:
                category = 'ordering_or_resource_barrier'
            elif op in {15, 16} or prev['op'] in {15, 16}:
                category = 'user_pointer_geometry_unknown'
            elif not geometry_known or not prev['geometry_binding_known']:
                category = 'unknown_geometry_binding'
            elif geometry == prev['geometry']:
                category = 'same_visible_geometry_signature_only'
            elif d['shaders_known'] and prev['shaders_known'] and material == prev['material']:
                category = 'different_visible_geometry_same_visible_material_only'
            else:
                category = 'other_or_changed_visible_state'
            pairs[category] += 1
            pair_records.append({'previous': prev['record'], 'next': index,
                                 'category': category,
                                 'opaque_state_changed': epoch != prev['opaque_epoch']})
        draws.append(d)
        barrier = False
        if op in {15, 16}:
            state.pop((23, (0,)), None)
        if op == 16:
            state.pop((25, ()), None)
    for d in draws:
        del d['geometry'], d['material']
    return {'records': last_index + 1, 'op_counts': dict(sorted(counts.items())),
            'draw_count': len(draws), 'adjacent_pair_partition': dict(pairs),
            'repeated_inline_setter_attempts': dict(repeat),
            'stream_frequency_records': frequencies, 'draws': draws, 'pairs': pair_records,
            'proven_safe_merge_draws': 0, 'true_eligible_draw_count': 'UNKNOWN',
            'limitations': ['Missing DIP a[5] primitive count and Rec.n',
                           'Missing bit-exact arena and large constant payloads',
                           'Missing initial stateblock, declaration and resource contents',
                           'Missing query/mutation history and setter HRESULTs',
                           'Known bindings describe recorded attempts only',
                           'No static-object, transform, skinning or lifetime proof']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.trace.read_bytes()
    result = audit(raw.decode('utf-8-sig'))
    result['source'] = {'path': str(args.trace), 'sha256': hashlib.sha256(raw).hexdigest(),
                        'bytes': len(raw)}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ['records', 'draw_count', 'adjacent_pair_partition',
                                         'repeated_inline_setter_attempts']}))


if __name__ == '__main__':
    main()
