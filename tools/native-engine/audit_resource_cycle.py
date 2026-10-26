"""Offline audit of the pinned resource-demand cycle; never opens a process.

Consumes existing CameraEvidence exports and existing cpu-phase-samples.json.
No ETL collection, hooks, game execution, configuration writes or timing probes.
Requires pefile and capstone. Exported game code stays outside this repository.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

EXPECTED_SHA = 'd46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1'
EXPORTS = ('entry-contracts.txt', 'parent-contracts.txt', 'scheduler-config.txt')
FUNCTIONS = {
    '00681e60': 'cycle source enumeration',
    '006f0190': 'deadline-driven cycle state machine',
    '00ac36f0': 'dependency reference collection',
    '00bb79a0': 'resource demand accumulation',
    '00c5d850': 'bounds-dependent demand accumulation',
    '00750cf0': 'demand-list sort',
    '00714b50': 'level selection and budget enforcement',
    '00609030': 'demand reset and change queue',
    '00bf27a0': 'texture and sampler preparation',
    '009f2030': 'intrusive child dispatch',
    '006ee720': 'dynamic buffer lock',
    '0071f690': 'dirty semantic dispatch',
}
CYCLE = set(list(FUNCTIONS)[:8])
CALLS = {
    0x006f02c3: 0x00ac36f0, 0x006f0388: 0x00bb79a0,
    0x00bb7ba7: 0x00c5d850, 0x006f04b0: 0x00750cf0,
    0x006f04d0: 0x00714b50, 0x006f04e3: 0x00609030,
}
CONTROLS = (
    ('SpeculativeTextureStreaming', 0x00ec43c0, 0x00fac818, 0x0110e2b4, 1),
    ('LimitTextureStreaming', 0x00ec4400, 0x00fb2804, 0x0110e2e0, 0),
    ('EnableTextureStreaming', 0x00ec4540, 0x00f5ae28, 0x0110e3f0, 1),
)


def read_exports(directory):
    owners = {}
    fingerprints = {}
    for name in EXPORTS:
        p = directory / name
        raw = p.read_bytes()
        lines = raw.decode('utf-8').splitlines()
        if not lines or lines[0].lower() != 'executable sha256: ' + EXPECTED_SHA:
            raise ValueError('Wrong Ghidra identity: ' + name)
        if lines[-1] != 'EVIDENCE COMPLETE':
            raise ValueError('Incomplete Ghidra export: ' + name)
        fingerprints[name] = hashlib.sha256(raw).hexdigest()
        owner = None
        for line in lines:
            if line.startswith('ADDRESS '):
                owner = None
            match = re.match(r'^FUNCTION ([0-9a-f]{8}) ', line)
            if match:
                owner = match[1]
            match = re.match(r'^([0-9a-f]{8}) [A-Z]', line)
            if owner in FUNCTIONS and match:
                pc = int(match[1], 16)
                if pc in owners and owners[pc] != owner:
                    raise ValueError('Conflicting instruction ownership')
                owners[pc] = owner
    missing = set(FUNCTIONS) - set(owners.values())
    if missing:
        raise ValueError('Missing function bodies: ' + ','.join(sorted(missing)))
    return owners, fingerprints


def audit_image(path):
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED_SHA:
        raise ValueError('Wrong executable SHA256; no address interpretation admitted')
    import capstone
    import pefile
    pe = pefile.PE(data=raw)
    if pe.FILE_HEADER.Machine != 0x14c or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
        raise ValueError('Unexpected PE32 identity')
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    def data(va, n):
        return pe.get_data(va - 0x400000, n)
    call_edges = []
    for site, target in CALLS.items():
        inst = next(md.disasm(data(site, 5), site), None)
        if inst is None or inst.mnemonic != 'call' or int(inst.op_str, 16) != target:
            raise ValueError('Call edge mismatch at ' + hex(site))
        call_edges.append({'site': hex(site), 'target': hex(target)})
    if int.from_bytes(data(0xff8d48, 4), 'little') != 0x6f0190:
        raise ValueError('Scheduler vtable slot mismatch')
    state_sites = []
    expected = {
        0x00bb7ae5: ('movss', 'dword ptr [esi + 0x48], xmm0'),
        0x00c5da96: ('movss', 'dword ptr [esi + 0x48], xmm0'),
        0x0060905f: ('xorps', 'xmm0, xmm0'),
        0x00609062: ('add', 'edi, 0x44'),
        0x00609065: ('movss', 'dword ptr [edi + 4], xmm0'),
        0x00609096: ('mov', 'byte ptr [edi + 2], al'),
        0x006090a2: ('and', 'byte ptr [edi], 0xfe'),
    }
    for site, operation in expected.items():
        inst = next(md.disasm(data(site, 15), site), None)
        if inst is None or (inst.mnemonic, inst.op_str) != operation:
            raise ValueError('Demand-cycle instruction mismatch at ' + hex(site))
        state_sites.append(hex(site))
    imports = {item.address: item.name for dll in pe.DIRECTORY_ENTRY_IMPORT for item in dll.imports}
    if imports.get(0x00eeb35c) != b'QueryPerformanceCounter':
        raise ValueError('Deadline clock import mismatch')
    for site in (0x006f03f2, 0x006f043a):
        inst = next(md.disasm(data(site, 6), site), None)
        if inst is None or (inst.mnemonic, inst.op_str) != ('call', 'dword ptr [0xeeb35c]'):
            raise ValueError('Deadline clock call mismatch')
    if data(0x00f38ad8, 9) != b'Graphics\0':
        raise ValueError('Control namespace mismatch')
    controls = []
    for name, entry, string, storage, default in CONTROLS:
        if data(string, len(name) + 1) != name.encode() + b'\0':
            raise ValueError('Control name mismatch')
        ops = [(i.mnemonic, i.op_str) for i in md.disasm(data(entry, 0x32), entry)]
        if ('push', hex(string)) not in ops or ('push', '0xf38ad8') not in ops:
            raise ValueError('Registration name reference missing')
        if ('mov', f'byte ptr [{hex(storage)}], {default}') not in ops:
            raise ValueError('Static default write missing')
        controls.append({'name': name, 'storage': hex(storage), 'static_default': default,
                         'effective_runtime_value': 'UNKNOWN'})
    return {'sha256': EXPECTED_SHA, 'call_edges': call_edges,
            'scheduler_vtable': '0x00ff8ce0', 'scheduler_slot_bytes': '0x68',
            'controls': controls, 'verified_demand_cycle_sites': state_sites}


def count_samples(path, owners, tid, phase, presents):
    if presents <= 0:
        raise ValueError('Positive raw phase Present denominator required')
    raw = path.read_bytes()
    j = json.loads(raw)
    if j.get('partial') is not False or j.get('exit') != 0:
        raise ValueError('Source extraction incomplete; this is not an ETW loss check')
    if phase not in {row[0] for row in j['phases']}:
        raise ValueError('Unknown phase')
    weights = dict.fromkeys(FUNCTIONS, 0)
    total = matched_rows = 0
    for row in j['pcs']:
        ph, thread, symbol, weight = row
        if ph != phase or thread != tid:
            continue
        matched_rows += 1
        if not isinstance(weight, int) or weight < 0:
            raise ValueError('Invalid sampled weight')
        m = re.fullmatch(r'DarknessII\.exe!0x([0-9a-fA-F]+)', symbol)
        if not m:
            continue
        total += weight
        owner = owners.get(int(m[1], 16))
        if owner is not None:
            weights[owner] += weight
    if not matched_rows or not total:
        raise ValueError('No native samples for requested thread/phase')
    return {
        'source_sha256': hashlib.sha256(raw).hexdigest(), 'thread': tid, 'phase': phase,
        'raw_phase_present_denominator': presents, 'native_leaf_samples': total,
        'native_leaf_estimated_ms_per_present': total / presents,
        'cycle_leaf_samples': sum(weights[f] for f in CYCLE),
        'cycle_leaf_estimated_ms_per_present': sum(weights[f] for f in CYCLE) / presents,
        'unclassified_native_leaf_samples': total - sum(weights.values()),
        'functions': [{'entry': f, 'role_inferred_from_dataflow': FUNCTIONS[f],
                       'samples': w, 'estimated_ms_per_present': w / presents}
                      for f, w in weights.items()],
        'limits': ['Nominal 1 ms sampled weights, not call counts or precise CPU durations',
                   'Leaf-only selected bodies; indirect descendants are not attributed',
                   'Input extraction completion does not establish loss-free ETW',
                   'Denominator supplied from the source phase receipt, not inferred',
                   'No runtime object identity, mutation epochs or safe replacement proven'],
    }


def validate_output(output, inputs):
    if output.suffix.lower() != '.json':
        raise ValueError('Output must be a derived .json report')
    for source in inputs:
        if output.resolve() == source.resolve() or (
                output.exists() and source.exists() and output.samefile(source)):
            raise ValueError('Output must not overwrite any evidence input')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--exports', type=Path, required=True)
    parser.add_argument('--samples', type=Path, required=True)
    parser.add_argument('--thread', type=int, required=True)
    parser.add_argument('--phase', required=True)
    parser.add_argument('--presents', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    validate_output(args.output, [args.exe, args.samples] +
                    [args.exports/name for name in EXPORTS])
    image = audit_image(args.exe)
    owners, hashes = read_exports(args.exports)
    samples = count_samples(args.samples, owners, args.thread, args.phase, args.presents)
    result = {'status': 'PASS_STATIC_AUDIT_ONLY', 'runtime_optimization_admitted': False,
              'image': image, 'export_sha256': hashes, 'samples': samples}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': result['status'], 'native_samples': samples['native_leaf_samples'],
                      'cycle_samples': samples['cycle_leaf_samples'],
                      'cycle_ms_estimate': samples['cycle_leaf_estimated_ms_per_present']}))


if __name__ == '__main__':
    main()
