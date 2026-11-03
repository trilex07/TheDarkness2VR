"""Read-only audit of effective C4D1FB10 constants in a validated capture.

This checks delivery of the existing correction, not lighting correctness or
ownership of every depth/normal input. Never starts a game or modifies a capture.
The native 0.95 conversion is pinned in native_projection_upload.h/its installer.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from check_matched_capture import validate


def audit(folder):
    folder = Path(folder)
    validate(folder)
    shader = folder / "ps-C4D1FB10.bin"
    if hashlib.sha256(shader.read_bytes()).hexdigest() != (
        "7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751"
    ):
        raise ValueError("shadow shader bytecode identity differs")
    rows = [json.loads(line) for line in (folder / "manifest.jsonl").read_text().splitlines()]
    cameras = [r for r in rows if r["kind"] == "selected_native_camera"]
    if len(cameras) != 1 or cameras[0]["camera_frame"] != cameras[0]["frame"]:
        raise ValueError("missing same-frame native camera")
    config = json.loads((folder / "effective-config.json").read_text())
    if config["shift_mode"] != 0 or config["projection_fix"] != 3 or config["no_inv_proj"]:
        raise ValueError("unsupported reconstruction policy")
    projection = np.array(cameras[0]["projection"], dtype=float).reshape(4, 4)
    projection[3, 2] *= .95  # actual native AD4021..AD4067 producer
    native_inverse = np.linalg.inv(projection)
    checks = []
    for row in rows:
        if row["kind"] != "lighting_ps_constants" or row["shader_hash"] != 0xC4D1FB10:
            continue
        if row["inverse_register"] != 27:
            raise ValueError("unexpected shadow reconstruction register")
        eye = config["eye_params"][row["eye"]]
        _, _, tx, ty, sx, sy, _ = eye["values"]
        expected = np.diag([1 / sx, 1 / sy, 1, 1]) @ native_inverse
        if eye["head_geometry"]:
            expected = np.array(eye["inverse_head_clip"]).reshape(4, 4) @ expected
        translation = (tx * expected[0, :3] / np.linalg.norm(expected[0, :3]) +
                       ty * expected[1, :3] / np.linalg.norm(expected[1, :3]))
        expected[:, :3] -= expected[:, 3:4] * translation
        actual = np.array(row["values"][108:124]).reshape(4, 4)
        error = float(np.max(np.abs(expected - actual)))
        if not np.isfinite(error) or error > 3e-5:
            raise ValueError(f"effective correction differs at eye {row['eye']} draw {row['draw']}: {error}")
        checks.append(error)
    if not checks:
        raise ValueError("no effective shadow reconstruction draws")
    return {"frame": cameras[0]["frame"], "effective_shadow_draws": len(checks),
            "max_absolute_matrix_error": max(checks),
            "existing_correction_delivered": True, "lighting_correctness_proven": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folders", type=Path, nargs="+")
    for folder in parser.parse_args().folders:
        print(json.dumps(audit(folder)))
