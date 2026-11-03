"""Reproject geometry through captured eye/PS matrices and shader input math.

This is an algebraic regression, not image acceptance. Uses private captures.
The expected result retains the native 0.95 reconstruction projection bias.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from check_matched_capture import validate


def audit(folder):
    validate(folder)
    records = [json.loads(x) for x in (folder / "manifest.jsonl").read_text().splitlines()]
    projection = np.array(next(x["projection"] for x in records
                              if x["kind"] == "selected_native_camera")).reshape(4, 4)
    config = json.loads((folder / "effective-config.json").read_text())
    rows = []
    for eye in range(2):
        params = config["eye_params"][eye]
        _, _, tx, ty, sx, sy, mode = params["values"]
        if mode != 0 or not params["head_geometry"]:
            raise ValueError("requires translated geometric-head capture")
        forward = projection @ np.diag([sx, sy, 1, 1]) @ np.array(params["head_clip"]).reshape(4, 4)
        forward[3, 0] += projection[0, 0] * sx * tx
        forward[3, 1] += projection[1, 1] * sy * ty
        matching = [x for x in records if x["kind"] == "lighting_ps_constants" and
                    x["eye"] == eye and x["shader_hash"] == 0xC4D1FB10]
        if not matching or not all(x["valid"] for x in matching):
            raise ValueError(f"{folder.name}: no valid effective C4D1FB10 draw for eye {eye}")
        matrix = np.array(matching[0]["values"][108:124]).reshape(4, 4)
        if not np.isfinite(matrix).all():
            raise ValueError("nonfinite inverse projection")
        biased = projection.copy()
        biased[3, 2] *= .95
        old_errors, candidate_errors = [], []
        for point in ([0, 0, 3, 1], [1, 0, 5, 1], [-1, -1, 10, 1]):
            clip = np.array(point) @ forward
            clip /= clip[3]
            original = clip.copy()
            original[:2] *= original[2]  # exact MUL at bytecode word 234
            expected = np.array(point) @ projection @ np.linalg.inv(biased)
            expected /= expected[3]
            for value, errors in ((original, old_errors), (clip, candidate_errors)):
                output = value @ matrix
                output /= output[3]
                errors.append(float(np.linalg.norm(output[:3] - expected[:3])))
        if not np.isfinite(old_errors + candidate_errors).all() or max(candidate_errors) > .002 or max(old_errors) < .02:
            raise ValueError("candidate regression or fixture does not expose input mismatch")
        rows.append({"eye": eye, "original_errors_game_units": old_errors,
                     "candidate_errors_game_units": candidate_errors})
    return {"capture": folder.name, "algebra_pass": True, "image_acceptance": False,
            "native_depth_bias_retained": True, "eyes": rows}


def audit_sampled_depth(folder):
    """Compare actual left-eye depth samples, without treating algebra as image QA."""
    validated = validate(folder)  # Native producer, copy ordering and bank restoration.
    records = [json.loads(x) for x in (folder / "manifest.jsonl").read_text().splitlines()]
    config = json.loads((folder / "effective-config.json").read_text())
    params = config["eye_params"][0]
    _, _, tx, ty, sx, sy, mode = params["values"]
    if mode != 0 or not params["head_geometry"]:
        raise ValueError("requires geometric-head gameplay capture")
    camera = next(x for x in records if x["kind"] == "selected_native_camera")
    projection = np.asarray(camera["projection"]).reshape(4, 4)
    forward = projection @ np.diag([sx, sy, 1, 1]) @ np.asarray(params["head_clip"]).reshape(4, 4)
    forward[3, 0] += projection[0, 0] * sx * tx
    forward[3, 1] += projection[1, 1] * sy * ty
    stage = next(x for x in records if x["kind"] == "pass_stage" and
                 x.get("ps_hash") == 0xC4D1FB10 and x.get("saved"))
    width, height = stage["width"], stage["height"]
    depth = np.fromfile(folder / stage["file"], dtype="<f4").reshape(height, width)
    yy, xx = np.mgrid[32:height-32:16, 32:width-32:16]
    z = depth[yy, xx].ravel()
    keep = np.isfinite(z) & (z > .1) & (z < .999)
    clip = np.column_stack([2*(xx.ravel()+.5)/width-1,
                            1-2*(yy.ravel()+.5)/height, z, np.ones(z.size)])[keep]
    if not len(clip):
        raise ValueError("no finite interior geometry depth samples")
    # Native viewport is [0.05, 1]. Undo its Z remap for the geometric oracle;
    # leave the captured PS inverse (including its native bias) untouched.
    source = validated["eyes"][0]["source"]
    minimum, maximum = source["viewport"][4:6]
    if not np.allclose((minimum, maximum), (.05, 1), rtol=0, atol=1e-7):
        raise ValueError("unsupported native depth range")
    native_clip = clip.copy()
    native_clip[:, 2] = (native_clip[:, 2]-minimum)/(maximum-minimum)
    expected = native_clip @ np.linalg.inv(forward)
    expected /= expected[:, 3, None]
    constants = next(x for x in records if x["kind"] == "lighting_ps_constants" and
                     x["eye"] == 0 and x["shader_hash"] == 0xC4D1FB10 and x["valid"])
    inverse = np.asarray(constants["values"][108:124]).reshape(4, 4)
    results = {}
    for name, multiply_depth in (("original", True), ("candidate", False)):
        value = clip.copy()
        if multiply_depth:
            value[:, :2] *= value[:, 2, None]
        output = value @ inverse
        output /= output[:, 3, None]
        error = np.linalg.norm(output[:, :3]-expected[:, :3], axis=1)
        if not np.isfinite(error).all():
            raise ValueError("nonfinite reconstruction error")
        results[name] = dict(zip(("median", "p90", "p99", "max"),
                                np.quantile(error, [.5, .9, .99, 1]).tolist()))
    return {"capture": folder.name, "eye": 0, "samples": len(clip),
            "errors_game_units": results, "native_depth_bias_retained": True,
            "image_acceptance": False, "fps_improvement_measured": False}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("folders", type=Path, nargs="+")
    p.add_argument("--sample-depth", action="store_true", help="use validated actual left-eye depth")
    args = p.parse_args()
    for folder in args.folders:
        print(json.dumps((audit_sampled_depth if args.sample_depth else audit)(folder)))
