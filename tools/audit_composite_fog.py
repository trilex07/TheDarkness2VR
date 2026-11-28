"""Measure pose-dependent fog-height drift using captured PS registers."""

import argparse
import hashlib
from itertools import product
import json
from pathlib import Path

import numpy as np

from check_matched_capture import validate
from disassemble_shader import disassemble

PS = 0x33DE98C1
VS = 0x24E3903D
PS_SHA = 'ae5d759a8d417d028248ec94176ef95dffeec3db64c5023b94ee6930958f7712'
VS_SHA = '98c9aff46ebb061c38fd5d4c27f3072a45abe354d254ac9cbac76fb0c7320f5d'


def verify_programs(folder):
    shader = folder / 'ps-A69BDC80.bin'
    vertex = folder / 'vs-24E3903D.bin'
    assert hashlib.sha256(shader.read_bytes()).hexdigest() == PS_SHA
    assert hashlib.sha256(vertex.read_bytes()).hexdigest() == VS_SHA
    ps = disassemble(shader)
    assert all(t in ps for t in ('texld r0, v0, s2', 'mul r0.yz, r0.x, v0.xzww',
                                  'mad r0.yz, r0.y, c10.xyww, r0.xzww',
                                  'mad r0.yz, r0.x, c12.xyww, r0',
                                  'add_pp r0.yz, r0, c13.xyww', 'mul_pp r0.y, r0.z, r0.y'))
    assert 'mov o1.zw, v0.xyxy' in disassemble(vertex)


def audit(folders):
    verify_programs(next(path for path in folders if (path / 'ps-A69BDC80.bin').is_file()))
    points = np.array([[z*x, z*y, z, 1.] for x, y, z in product(
        np.linspace(-.9, .4, 53), np.linspace(-.4, .4, 41), [.35, .5, 1, 2, 4, 8, 16, 32])])
    measurements, visibility, frame_ids, poses = [], [], [], []
    native_camera = None
    for folder in folders:
        capture = validate(folder)
        frame_ids.append(capture['frame'])
        rows = [json.loads(line) for line in (folder / 'manifest.jsonl').read_text().splitlines()]
        camera = next(row for row in rows if row['kind'] == 'selected_native_camera')
        if native_camera is None:
            native_camera = camera
        assert camera['view_projection'] == native_camera['view_projection']
        assert camera['projection'] == native_camera['projection']
        p = np.asarray(camera['projection'], dtype=float).reshape(4, 4)
        for eye, params in enumerate(capture['config']['eye_params']):
            _, _, tx, ty, sx, sy, mode = params['values']
            assert mode == 0 and params['head_geometry']
            poses.append(tuple(params['head_clip']))
            forward = p @ np.diag([sx, sy, 1, 1]) @ np.asarray(params['head_clip']).reshape(4, 4)
            forward[3, 0] += p[0, 0]*sx*tx
            forward[3, 1] += p[1, 1]*sy*ty
            homogeneous = points @ forward
            clip = homogeneous / homogeneous[:, 3, None]
            depth = clip.copy()
            low, high = capture['eyes'][eye]['source']['viewport'][4:6]
            assert np.allclose((low, high), (.05, 1), rtol=0, atol=1e-7)
            depth[:, 2] = low + (high-low)*clip[:, 2]
            legacy = depth.copy()
            legacy[:, :2] *= depth[:, 2, None]
            bank, = [r for r in rows if r['kind'] == 'lighting_ps_constants' and
                     r['eye'] == eye and r['shader_hash'] == PS and r['valid'] and r['inverse_register'] == 10]
            draw, = [r for r in rows if r['kind'] == 'draw' and r['eye'] == eye and r['draw'] == bank['draw']]
            assert draw['shader_hash'] == VS
            inverse = np.asarray(bank['values'][40:56]).reshape(4, 4)
            outputs = [value @ inverse[:, (1, 3)] for value in (legacy, depth, clip)]
            measurements.append([height[:, 0] / height[:, 1] for height in outputs])
            visibility.append((homogeneous[:, 3] > 0) & np.all(abs(clip[:, :2]) < .95, axis=1) &
                              (depth[:, 2] > .1) & (depth[:, 2] < .998))
    assert len(set(poses)) >= 2
    visible = np.all(visibility, axis=0)
    heights = np.asarray(measurements)[:, :, visible]
    assert np.count_nonzero(visible) >= 100 and np.isfinite(heights).all()
    drift = np.ptp(heights, axis=0)
    methods = ('native_depth_xy', 'unscaled_xy', 'unscaled_xy_and_normalized_depth')
    return dict(frames=frame_ids, eye_pose_count=len(poses), visible_points=int(np.sum(visible)),
                range_game_units={method: dict(zip(('median', 'p90', 'p99', 'max'),
                                  np.quantile(samples, [.5, .9, .99, 1]).tolist()))
                                  for method, samples in zip(methods, drift)},
                method='synthetic geometry projected through captured native camera and effective PS banks',
                actual_fog_s2_depth_sampled=False, visual_result_verified=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, nargs='+')
    args = parser.parse_args()
    if len(args.captures) < 2:
        parser.error('provide at least two matched frames of the same native camera')
    print(json.dumps(audit(args.captures), indent=2))
