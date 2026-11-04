"""Compare a captured shadow mask at depth-consistent reprojected world points.

Requires the same native camera and exact light constants. This measures one
captured shadow consumer; it is not headset acceptance or a full-scene score.
"""
import argparse
import json
import sys
from pathlib import Path
import numpy as np
from check_matched_capture import validate


def load(folder):
    validated = validate(folder)
    records = [json.loads(x) for x in (folder / 'manifest.jsonl').read_text().splitlines()]
    camera = next(x for x in records if x['kind'] == 'selected_native_camera')
    params = json.loads((folder / 'effective-config.json').read_text())['eye_params'][0]
    _, _, tx, ty, sx, sy, mode = params['values']
    if mode or not params['head_geometry']:
        raise ValueError('requires geometric-head gameplay')
    p = np.array(camera['projection']).reshape(4, 4)
    forward = p @ np.diag([sx, sy, 1, 1]) @ np.array(params['head_clip']).reshape(4, 4)
    forward[3, 0] += p[0, 0] * sx * tx
    forward[3, 1] += p[1, 1] * sy * ty
    stages = [x for x in records if x['kind'] == 'pass_stage' and x.get('ps_hash') == 0xC4D1FB10 and x.get('saved')]
    depth_stage = next(x for x in stages if x['phase'] == 'before_draw_input' and x['format'] == 114)
    mask_stage = next(x for x in stages if x['phase'] == 'after_draw' and x['format'] == 113 and x['last_draw'] == depth_stage['last_draw'])
    w, h = depth_stage['width'], depth_stage['height']
    if (mask_stage['width'], mask_stage['height']) != (w, h):
        raise ValueError('mask/depth dimensions differ')
    constants = next(x for x in records if x['kind'] == 'lighting_ps_constants' and x['eye'] == 0 and x['draw'] == depth_stage['last_draw'] and x['valid'])
    depth = np.fromfile(folder / depth_stage['file'], dtype='<f4').reshape(h, w)
    before_stage = next(x for x in stages if x['phase'] == 'shadow_before_draw' and x['last_draw'] == depth_stage['last_draw'])
    blend = next(x for x in records if x['kind'] == 'shadow_blend' and x['eye'] == 0 and x['draw'] == depth_stage['last_draw'] and x['valid'])
    state = blend['write_enable_src_dst_op_separate_asrc_adst_aop']
    # D3DBLEND_DESTCOLOR, ZERO, ADD: after = shader_output * before.
    # Never interpret accumulated lighting as the grayscale shader output.
    if (state[:6] != [15, 1, 9, 1, 1, 0] or before_stage['format'] != 113 or
            before_stage['rt'] != mask_stage['rt'] or
            (before_stage['width'], before_stage['height']) != (w, h)):
        raise ValueError('unsupported shadow blend or prior target')
    rgba = np.fromfile(folder / mask_stage['file'], dtype='<f2').reshape(h, w, 4).astype(np.float32)
    prior = np.fromfile(folder / before_stage['file'], dtype='<f2').reshape(h, w, 4).astype(np.float32)
    channel = np.argmax(prior, axis=2)[..., None]
    denominator = np.take_along_axis(prior, channel, axis=2)[..., 0]
    numerator = np.take_along_axis(rgba, channel, axis=2)[..., 0]
    valid = np.isfinite(prior).all(2) & np.isfinite(rgba).all(2) & (denominator > .001)
    mask = np.divide(numerator, denominator, out=np.full_like(denominator, np.nan), where=valid)
    valid &= (mask >= 0) & (mask <= 1.002)
    return camera, forward, np.array(constants['values']).reshape(224, 4), depth, mask, valid, validated['eyes'][0]['source']['viewport'][4:6]


def compare(a, b):
    ca, fa, ka, da, ma, valid_a, za = load(a)
    cb, fb, kb, db, mb, valid_b, zb = load(b)
    if not np.allclose(ca['view_projection'], cb['view_projection'], rtol=0, atol=1e-5):
        raise ValueError('native camera moved')
    if not np.allclose(ca['projection'], cb['projection'], rtol=0, atol=1e-7):
        raise ValueError('native projection changed')
    # These are the exact native shader CTAB inputs. c2..15 and c20..23 are
    # DEF literals, so unrelated device-bank differences there are immaterial.
    registers = [0, 1, 16, 17, 18, 19]
    if not np.allclose(ka[registers], kb[registers], rtol=0, atol=1e-5):
        raise ValueError('different light or shadow projection')
    if np.allclose(fa, fb, rtol=0, atol=1e-5):
        raise ValueError('head projection did not change')
    if da.shape != db.shape or not np.allclose(za, zb):
        raise ValueError('source dimensions or viewport depth range changed')
    h, w = da.shape
    # Sample actual shadow contribution, not the vast unchanged background.
    y, x = np.nonzero(valid_a & (ma < .98))
    if len(x) < 100:
        raise ValueError('capture does not expose enough shadow contribution')
    stride = max(1, len(x)//200000)
    x, y = x[::stride], y[::stride]
    z = da[y, x]
    clip = np.column_stack([2*(x+.5)/w-1, 1-2*(y+.5)/h, (z-za[0])/(za[1]-za[0]), np.ones(len(x))])
    projected = clip @ np.linalg.inv(fa) @ fb
    positive = projected[:, 3] > 0
    projected /= projected[:, 3, None]
    bx = np.rint((projected[:, 0]+1)*w/2-.5).astype(int)
    by = np.rint((1-projected[:, 1])*h/2-.5).astype(int)
    inside = positive & (bx >= 32) & (bx < w-32) & (by >= 32) & (by < h-32) & np.isfinite(z) & (z > .1) & (z < .999)
    x, y, bx, by = (v[inside] for v in (x, y, bx, by))
    expected_z = projected[inside, 2]*(zb[1]-zb[0])+zb[0]
    visible = np.abs(db[by, bx]-expected_z) < 0.00005
    va, vb = ma[y, x], mb[by, bx]
    mask = visible & valid_a[y, x] & valid_b[by, bx]
    error = np.abs(va[mask]-vb[mask])
    if len(error) < 100:
        raise ValueError('insufficient depth-consistent shadow contribution samples')
    return dict(captures=[a.name, b.name], samples=len(error), absolute_mask_error=dict(zip(['median', 'p90', 'p99', 'max'], np.quantile(error, [.5, .9, .99, 1]).tolist())), mean=float(error.mean()), fraction_over_0_1=float(np.mean(error > .1)), headset_acceptance=False)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('a', type=Path)
    p.add_argument('b', type=Path)
    args = p.parse_args()
    try:
        print(json.dumps(compare(args.a, args.b)))
    except (ValueError, KeyError, StopIteration, OSError) as error:
        print(f'REJECT: missing or incompatible shadow evidence: {error}', file=sys.stderr)
        raise SystemExit(2)
