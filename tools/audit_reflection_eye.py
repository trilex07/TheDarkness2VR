"""Read-only model-space reflection-eye audit of a matched frame.

This identifies a constant/geometry camera mismatch, not the pixels responsible
for a reported floor artifact. No game, shader mutation or deployment occurs.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from check_matched_capture import validate


VS = 0x12CCC8DB
VS_SHA = '223298007ec4e1942d8e4ec600c508b9e677a2b3afa8f2090047579ea67bcca3'


def center(matrix):
    # Row-vector perspective: camera origin projects to [0,0,k,0].
    value = np.array([0., 0., 1., 0.]) @ np.linalg.inv(matrix)
    if not np.isfinite(value).all() or abs(value[3]) < 1e-8:
        raise ValueError('No finite perspective camera center')
    return value[:3] / value[3]


def audit(folder):
    folder = Path(folder)
    validate(folder)
    shader = folder / 'vs-12CCC8DB.bin'
    if hashlib.sha256(shader.read_bytes()).hexdigest() != VS_SHA:
        raise ValueError('Unknown reflection vertex shader')
    config = json.loads((folder / 'effective-config.json').read_bytes())
    if (config['shift_mode'] != 0 or config['projection_fix'] != 3 or
            not config['head_geometry']):
        raise ValueError('Unsupported stereo transform policy')
    rows = [json.loads(line) for line in (folder / 'manifest.jsonl').read_text().splitlines()]
    cameras = [row for row in rows if row['kind'] == 'selected_native_camera']
    if len(cameras) != 1 or cameras[0]['camera_frame'] != cameras[0]['frame']:
        raise ValueError('Missing same-frame projection coefficients')
    projection = np.array(cameras[0]['projection']).reshape(4, 4)
    px, py = projection[0, 0], projection[1, 1]
    if not np.isfinite([px, py]).all() or min(px, py) <= 0:
        raise ValueError('Invalid native projection scale')
    banks = (folder / 'vs-banks.bin').read_bytes()
    checks = []
    for draw in rows:
        if draw['kind'] != 'draw' or draw['shader_hash'] != VS:
            continue
        eye = draw['eye']
        params = config['eye_params'][eye]
        if not params['head_geometry'] or params['mode'] != 0:
            raise ValueError('Unsupported per-eye policy')
        bank = np.frombuffer(banks, dtype='<f4', count=1024,
                             offset=draw['bank_offset']).reshape(256, 4)
        effective = bank[:4].astype(float)
        if not np.isfinite(bank[:9]).all():
            raise ValueError('Nonfinite captured constants')
        _, _, tx, ty, sx, sy, _ = params['values']
        if min(sx, sy) <= 0 or not np.isfinite(params['values']).all():
            raise ValueError('Invalid eye transform')
        # Undo the actual stereo_geometry.h order: eye translation, head clip,
        # then render FOV. Do not use the late selected camera's VP as an object
        # ownership witness; it can differ from the camera used by this draw.
        raw = effective.copy()
        raw[3, 0] -= px * tx * sx
        raw[3, 1] -= py * ty * sy
        raw = raw @ np.array(params['inverse_head_clip']).reshape(4, 4)
        raw[:, 0] /= sx
        raw[:, 1] /= sy
        native = center(raw)
        actual = center(effective)
        checks.append(dict(eye=eye, draw=draw['draw'],
            native_constant_error=float(np.linalg.norm(native - bank[8, :3])),
            rendered_eye_difference=float(np.linalg.norm(actual - bank[8, :3]))))
    if not checks or {row['eye'] for row in checks} != {0, 1}:
        raise ValueError('Need consuming draws in both eyes')
    result = dict(frame=cameras[0]['frame'], eyes=[],
                  floor_pixel_ownership_proven=False, visual_fix_proven=False)
    for eye in (0, 1):
        values = [row for row in checks if row['eye'] == eye]
        native_errors = [row['native_constant_error'] for row in values]
        # Float WVP inversion magnifies precision error for distant/scaled
        # objects. Reject rather than label unrelated constants as an eye.
        if max(native_errors) > .001:
            raise ValueError('Native eye/geometry identity is not established')
        result['eyes'].append(dict(eye=eye, draws=len(values),
            max_native_constant_error=max(native_errors),
            median_rendered_eye_difference=float(np.median(
                [row['rendered_eye_difference'] for row in values]))))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folders', type=Path, nargs='+')
    for folder in parser.parse_args().folders:
        print(json.dumps(audit(folder)))
