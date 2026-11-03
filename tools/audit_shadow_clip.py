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


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("folders", type=Path, nargs="+")
    for folder in p.parse_args().folders:
        print(json.dumps(audit(folder)))
