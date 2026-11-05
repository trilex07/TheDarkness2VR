"""Check effective clip-to-texture constants in explicit private frame captures.

This verifies register consumption, not visual acceptance of an impact effect.
No captures or proprietary shader bytecode are written into the repository.
"""
import argparse
import json
import struct
from pathlib import Path


def objects(text):
    decoder = json.JSONDecoder()
    while text.strip():
        text = text.lstrip()
        row, end = decoder.raw_decode(text)
        yield row
        text = text[end:]


def inspect(directory):
    rows = list(objects((directory / 'manifest.jsonl').read_text()))
    header = next(row for row in rows if row['kind'] == 'header')
    banks = (directory / 'vs-banks.bin').read_bytes()
    samples = []
    # Exact native VS9E39DAF1: audited c0 -> clip -> c4 -> depth UV.
    for row in rows:
        if row['kind'] != 'draw' or row['shader_hash'] != 0x9E39DAF1:
            continue
        matrix = struct.unpack_from('<16f', banks, row['bank_offset'] + 64)
        expected = [.5, 0, 0, 0, 0, -.5, 0, 0, 0, 0, 1, 0,
                    matrix[12], matrix[13], 0, 1]
        error = max(abs(a-b) for a, b in zip(matrix, expected))
        valid = error < 1e-6 and all(.5 <= matrix[i] < .501 for i in (12, 13))
        samples.append(dict(eye=row['eye'], draw=row['draw'], valid=valid,
                            error=error, c4=list(matrix)))
    return dict(path=str(directory), dll=header['dll_sha256'], samples=samples,
                status='PASS' if samples and all(x['valid'] for x in samples)
                else 'NOT_OBSERVED' if not samples else 'FAIL')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reports = [inspect(path) for path in args.captures]
    args.output.write_text(json.dumps(reports, indent=2))
    for report in reports:
        print(report['status'], report['path'], 'draws', len(report['samples']))
    if any(report['status'] != 'PASS' for report in reports):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
