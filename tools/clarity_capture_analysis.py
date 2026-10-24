#!/usr/bin/env python3
"""Offline same-pixel capture comparison and FNV-1a D3D9 shader fingerprinting.

Image comparison is deliberately unregistered: the two files must represent the
same eye, camera, scene, exposure, and output dimensions. Use --roi to compare a
known static area. Metrics describe pixel/edge differences; they do not identify
their cause or replace headset inspection.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path


def read_bmp(path: Path) -> tuple[int, int, bytearray]:
    with path.open("rb") as stream:
        header = stream.read(14)
        if len(header) != 14:
            raise ValueError(f"{path}: truncated BMP file header")
        magic, _file_size, _reserved1, _reserved2, pixel_offset = struct.unpack("<2sIHHI", header)
        if magic != b"BM":
            raise ValueError(f"{path}: not a BMP file")
        size_bytes = stream.read(4)
        if len(size_bytes) != 4:
            raise ValueError(f"{path}: truncated DIB header")
        dib_size = struct.unpack("<I", size_bytes)[0]
        if dib_size < 40:
            raise ValueError(f"{path}: unsupported BMP DIB header size {dib_size}")
        dib_rest = stream.read(dib_size - 4)
        if len(dib_rest) != dib_size - 4:
            raise ValueError(f"{path}: truncated DIB header")
        (width, signed_height, planes, bits_per_pixel, compression, _image_size,
         _xppm, _yppm, _colors_used, _important) = struct.unpack_from("<iiHHIIiiII", dib_rest)
        if width <= 0 or signed_height == 0 or planes != 1:
            raise ValueError(f"{path}: invalid BMP dimensions or plane count")
        if bits_per_pixel not in (24, 32) or compression != 0:
            raise ValueError(f"{path}: only uncompressed 24/32-bit BMP is supported")
        height = abs(signed_height)
        bytes_per_pixel = bits_per_pixel // 8
        row_stride = ((width * bits_per_pixel + 31) // 32) * 4
        stream.seek(pixel_offset)
        raw = stream.read(row_stride * height)
        if len(raw) != row_stride * height:
            raise ValueError(f"{path}: truncated BMP pixel data")

    gray = bytearray(width * height)
    top_down = signed_height < 0
    for y in range(height):
        source_y = y if top_down else height - 1 - y
        row_start = source_y * row_stride
        gray_start = y * width
        for x in range(width):
            offset = row_start + x * bytes_per_pixel
            blue, green, red = raw[offset], raw[offset + 1], raw[offset + 2]
            gray[gray_start + x] = (19 * blue + 183 * green + 54 * red + 128) >> 8
    return width, height, gray


def compare_same_pixels(a: bytearray, b: bytearray, width: int, height: int,
                        roi: tuple[int, int, int, int]) -> dict[str, float | int | list[int] | str]:
    x0, y0, roi_w, roi_h = roi
    if roi_w <= 0 or roi_h <= 0 or x0 < 0 or y0 < 0 or x0 + roi_w > width or y0 + roi_h > height:
        raise ValueError(f"ROI {roi} is outside {width}x{height}")
    abs_sum = 0
    square_sum = 0
    max_diff = 0
    count = roi_w * roi_h
    grad_a = grad_b = 0
    lap_a = lap_b = 0
    metric_pixels = 0

    for y in range(y0, y0 + roi_h):
        row = y * width
        for x in range(x0, x0 + roi_w):
            i = row + x
            delta = abs(a[i] - b[i])
            abs_sum += delta
            square_sum += delta * delta
            max_diff = max(max_diff, delta)
            if x + 1 < x0 + roi_w and y + 1 < y0 + roi_h:
                gx_a = int(a[i + 1]) - int(a[i])
                gy_a = int(a[i + width]) - int(a[i])
                gx_b = int(b[i + 1]) - int(b[i])
                gy_b = int(b[i + width]) - int(b[i])
                grad_a += gx_a * gx_a + gy_a * gy_a
                grad_b += gx_b * gx_b + gy_b * gy_b
                if x > x0 and y > y0 and x + 1 < x0 + roi_w and y + 1 < y0 + roi_h:
                    lap1 = 4 * int(a[i]) - int(a[i - 1]) - int(a[i + 1]) - int(a[i - width]) - int(a[i + width])
                    lap2 = 4 * int(b[i]) - int(b[i - 1]) - int(b[i + 1]) - int(b[i - width]) - int(b[i + width])
                    lap_a += lap1 * lap1
                    lap_b += lap2 * lap2
                    metric_pixels += 1

    return {
        "width": width,
        "height": height,
        "roi_xywh": [x0, y0, roi_w, roi_h],
        "coordinate_rule": "same integer pixel coordinates; no registration or warping",
        "pixel_mae": abs_sum / count,
        "pixel_rmse": math.sqrt(square_sum / count),
        "pixel_max_abs": max_diff,
        "gradient_rms_a": math.sqrt(grad_a / (2 * max(1, (roi_w - 1) * (roi_h - 1)))),
        "gradient_rms_b": math.sqrt(grad_b / (2 * max(1, (roi_w - 1) * (roi_h - 1)))),
        "laplacian_rms_a": math.sqrt(lap_a / max(1, metric_pixels)),
        "laplacian_rms_b": math.sqrt(lap_b / max(1, metric_pixels)),
        "laplacian_rms_delta_pct": (100.0 * (math.sqrt(lap_b / metric_pixels) /
            math.sqrt(lap_a / metric_pixels) - 1.0)) if metric_pixels and lap_a else None,
    }


def fnv1a32(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def measure_horizontal_disparity(left: bytearray, right: bytearray, image_width: int,
                                image_height: int, roi: tuple[int, int, int, int],
                                max_shift: int) -> dict[str, float | int | list[int] | str]:
    x0, y0, roi_w, roi_h = roi
    if roi_w < 3 or roi_h < 3 or x0 < 0 or y0 < 0 or x0 + roi_w > image_width or y0 + roi_h > image_height:
        raise ValueError(f"ROI {roi} is outside {image_width}x{image_height} or too small")
    if max_shift < 0 or x0 - max_shift < 0 or x0 + roi_w + max_shift > image_width:
        raise ValueError("horizontal search range does not fit in the right-eye image")

    patch: list[int] = []
    for y in range(y0, y0 + roi_h):
        start = y * image_width + x0
        patch.extend(left[start:start + roi_w])
    mean_a = sum(patch) / len(patch)
    centered_a = [value - mean_a for value in patch]
    variance_a = sum(value * value for value in centered_a)
    if variance_a <= 1e-9:
        raise ValueError("left-eye ROI has no contrast; choose a textured or high-contrast feature")

    best_shift = 0
    best_score = -2.0
    second_score = -2.0
    for shift in range(-max_shift, max_shift + 1):
        values_b: list[int] = []
        for y in range(y0, y0 + roi_h):
            start = y * image_width + x0 + shift
            values_b.extend(right[start:start + roi_w])
        mean_b = sum(values_b) / len(values_b)
        centered_b = [value - mean_b for value in values_b]
        variance_b = sum(value * value for value in centered_b)
        if variance_b <= 1e-9:
            score = -1.0
        else:
            covariance = sum(a * b for a, b in zip(centered_a, centered_b))
            score = covariance / math.sqrt(variance_a * variance_b)
        if score > best_score:
            second_score = best_score
            best_shift, best_score = shift, score
        elif score > second_score:
            second_score = score

    return {
        "width": image_width,
        "height": image_height,
        "roi_xywh_left": [x0, y0, roi_w, roi_h],
        "best_right_minus_left_px": best_shift,
        "left_minus_right_px": -best_shift,
        "ncc": best_score,
        "runner_up_ncc": second_score,
        "coordinate_rule": "same-row horizontal template search; no scaling or vertical registration",
    }


def run_self_test() -> None:
    width, height = 64, 64
    sharp = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            sharp[y * width + x] = 255 if (x // 2) % 2 else 0
    blurred = bytearray(sharp)
    for y in range(height):
        for x in range(1, width - 1):
            i = y * width + x
            blurred[i] = (int(sharp[i - 1]) + 2 * int(sharp[i]) + int(sharp[i + 1])) // 4
    result = compare_same_pixels(sharp, blurred, width, height, (2, 2, 60, 60))
    if result["pixel_mae"] <= 0 or result["laplacian_rms_b"] >= result["laplacian_rms_a"]:
        raise AssertionError("synthetic blur fixture did not lower high-frequency response")
    identical = compare_same_pixels(sharp, sharp, width, height, (0, 0, width, height))
    if identical["pixel_mae"] != 0 or identical["pixel_max_abs"] != 0:
        raise AssertionError("identical-image fixture was not pixel-identical")

    pattern = bytearray(((x * 37 + y * 61 + x * y * 13) % 256) for y in range(height) for x in range(width))
    shifted = bytearray(width * height)
    shift = 5
    for y in range(height):
        for x in range(width - shift):
            shifted[y * width + x] = pattern[y * width + x + shift]
    disparity = measure_horizontal_disparity(pattern, shifted, width, height, (15, 12, 20, 24), 8)
    if disparity["best_right_minus_left_px"] != -shift or disparity["ncc"] < 0.999:
        raise AssertionError("synthetic stereo pair returned the wrong horizontal disparity")
    print("PASS: same-pixel image metrics detect synthetic blur; template search recovers a known disparity")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("self-test", help="run synthetic image-metric checks")
    image = commands.add_parser("compare", help="compare same-eye captures at identical pixel coordinates")
    image.add_argument("image_a", type=Path)
    image.add_argument("image_b", type=Path)
    image.add_argument("--roi", nargs=4, type=int, metavar=("X", "Y", "WIDTH", "HEIGHT"))
    image.add_argument("--json", action="store_true", help="emit JSON")
    disparity = commands.add_parser("measure-disparity", help="match one left-eye ROI horizontally in the right-eye image")
    disparity.add_argument("left_image", type=Path)
    disparity.add_argument("right_image", type=Path)
    disparity.add_argument("--roi", nargs=4, type=int, required=True, metavar=("X", "Y", "WIDTH", "HEIGHT"))
    disparity.add_argument("--max-shift", type=int, default=80)
    disparity.add_argument("--px-effective", type=float, help="rendered horizontal focal coefficient after FOV scale and crop")
    disparity.add_argument("--distance-game", type=float, help="known camera-to-feature distance in Evolution Engine units")
    disparity.add_argument("--xr-ipd-m", type=float, help="inter-eye distance from xrLocateViews, in metres")
    disparity.add_argument("--json", action="store_true", help="emit JSON")
    shader = commands.add_parser("shader-hash", help="compute the mod's 32-bit FNV-1a hash over a shader bytecode file")
    shader.add_argument("bytecode", type=Path)
    args = parser.parse_args()

    try:
        if args.command == "self-test":
            run_self_test()
            return 0
        if args.command == "shader-hash":
            data = args.bytecode.read_bytes()
            if len(data) % 4:
                raise ValueError("shader bytecode length is not DWORD-aligned")
            print(json.dumps({"path": str(args.bytecode), "bytes": len(data),
                              "fnv1a32": f"{fnv1a32(data):08X}"}, indent=2))
            return 0
        if args.command == "measure-disparity":
            wl, hl, left = read_bmp(args.left_image)
            wr, hr, right = read_bmp(args.right_image)
            if (wl, hl) != (wr, hr):
                raise ValueError(f"capture dimensions differ: {wl}x{hl} vs {wr}x{hr}")
            result = measure_horizontal_disparity(left, right, wl, hl, tuple(args.roi), args.max_shift)
            result["left_image"] = str(args.left_image)
            result["right_image"] = str(args.right_image)
            if args.distance_game is not None or args.xr_ipd_m is not None or args.px_effective is not None:
                if not args.distance_game or not args.xr_ipd_m or not args.px_effective:
                    raise ValueError("scale recovery requires --distance-game, --px-effective, and --xr-ipd-m")
                if args.distance_game <= 0 or args.px_effective <= 0 or args.xr_ipd_m <= 0:
                    raise ValueError("scale recovery inputs must be positive")
                game_ipd = (2.0 * args.distance_game * abs(float(result["left_minus_right_px"]))) / (wl * args.px_effective)
                result["estimated_ipd_game_units"] = game_ipd
                result["estimated_game_units_per_meter"] = game_ipd / args.xr_ipd_m
                result["scale_note"] = "valid for parallel-translation stereo and a correctly identified feature at the supplied game-space distance"
            print(json.dumps(result, indent=2) if args.json else "\n".join(f"{k}: {v}" for k, v in result.items()))
            return 0
        wa, ha, a = read_bmp(args.image_a)
        wb, hb, b = read_bmp(args.image_b)
        if (wa, ha) != (wb, hb):
            raise ValueError(f"capture dimensions differ: {wa}x{ha} vs {wb}x{hb}; resample/crop explicitly first")
        roi = tuple(args.roi) if args.roi else (0, 0, wa, ha)
        result = compare_same_pixels(a, b, wa, ha, roi)
        result["image_a"] = str(args.image_a)
        result["image_b"] = str(args.image_b)
        print(json.dumps(result, indent=2) if args.json else "\n".join(f"{k}: {v}" for k, v in result.items()))
        return 0
    except (OSError, ValueError, AssertionError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
