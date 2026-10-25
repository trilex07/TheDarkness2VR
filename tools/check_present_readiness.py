"""Read-only gate for the counter-covered prefix of a diagnostic mono log.

Never treats legacy sampled HRESULT logs as complete return evidence. This does
not prove graceful shutdown, loaded gameplay, or headset correctness.
"""
import argparse
import json
import re
from pathlib import Path

ROW = re.compile(r'\[DiagnosticEnginePresent\] entries=(\d+) forwarded=(\d+) returned=(\d+) recursive=(\d+) hr=([0-9A-Fa-f]+) sc=(\w+) tid=(\d+) fault=(\d+)(?: ok=(\d+) statuses=(\d+) errors=(\d+))?')


def assess(text):
    reasons = []
    rows = list(ROW.finditer(text))
    if not rows:
        reasons.append('no Present return observations')
    if 'OpenXR=disabled' not in text or not re.search(r'DisableOpenXR=1 SubmitMode=4 StereoMode=0\b', text):
        reasons.append('effective mono/OpenXR-off configuration missing')
    if not re.search(r'\[DiagnosticEnginePresent\] installed=1 rollbackUncertain=0\b', text):
        reasons.append('verified installation receipt missing')
    identities = re.findall(r'\[DiagnosticEnginePresent\] identity sc=(\w+).*?caller=(\w+) exactCaller=1 tid=(\d+)', text)
    if not identities:
        reasons.append('exact engine caller identity missing')
    if '!!! EXCEPTION' in text or '[DiagnosticEnginePresent] FAIL' in text:
        reasons.append('exception or dispatch failure')
    last_by_thread = {}
    for m in rows:
        entries, forwarded, returned, recursive = map(int, m.group(1, 2, 3, 4))
        hr, sc, tid, fault = m.group(5, 6, 7, 8)
        if m.group(9) is None:
            reasons.append('legacy sampled HRESULTs: full return counters missing')
            continue
        ok, statuses, errors = map(int, m.group(9, 10, 11))
        if entries <= 0 or not entries == forwarded == returned or returned != ok + statuses + errors:
            reasons.append('inconsistent forwarding/result counters')
        if recursive or int(fault) or errors or statuses or int(hr, 16):
            reasons.append('non-D3D_OK result, recursion or latched fault')
        if not any(s == sc and t == tid for s, _, t in identities):
            reasons.append('return does not match observed swapchain/thread')
        current = (entries, ok, statuses, errors)
        previous = last_by_thread.get(tid, (0, 0, 0, 0))
        if any(a < b for a, b in zip(current, previous)):
            reasons.append('counter regression')
        last_by_thread[tid] = current
    return dict(status='NO_GO' if reasons else 'PASS_OBSERVED_PREFIX',
                reasons=sorted(set(reasons)), return_rows=len(rows),
                covered_returns=sum(v[0] for v in last_by_thread.values()),
                full_session_proven=False, gameplay_proven=False, vr_approved=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    result = assess(args.log.read_text(encoding='utf-8', errors='replace'))
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'PASS_OBSERVED_PREFIX' else 2


if __name__ == '__main__':
    raise SystemExit(main())
