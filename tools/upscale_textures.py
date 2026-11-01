#!/usr/bin/env python3
"""
Real-ESRGAN x4 upscaling for Darkness2VR extracted textures.
Runs locally with PyTorch + RTX GPU (no basicsr dependency).

Usage:
    python tools/upscale_textures.py --source extracted_textures/source --dest extracted_textures --scales 2 4 8

Output structure (mirrors prepare_hires_textures.ps1):
    extracted_textures/
        2k/source/{weapons,tentacles,enemies}/   <- original resolution copy
        2k/{weapons,tentacles,enemies}/          <- 2048px max side
        4k/{weapons,tentacles,enemies}/          <- 4096px max side
        8k/{weapons,tentacles,enemies}/          <- 8192px max side
"""

import os
import sys
import argparse
import threading
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image
import numpy as np


# ---------------------------------------------------------------------------
# Real-ESRGAN RRDB architecture (x4plus), pure PyTorch
# ---------------------------------------------------------------------------
class DenseBlock(nn.Module):
    def __init__(self, nf=64, gc=32):
        super().__init__()
        self.conv1 = nn.Conv2d(nf, gc, 3, 1, 1, bias=True)
        self.conv2 = nn.Conv2d(nf + gc, gc, 3, 1, 1, bias=True)
        self.conv3 = nn.Conv2d(nf + 2 * gc, gc, 3, 1, 1, bias=True)
        self.conv4 = nn.Conv2d(nf + 3 * gc, gc, 3, 1, 1, bias=True)
        self.conv5 = nn.Conv2d(nf + 4 * gc, nf, 3, 1, 1, bias=True)
        self.lrelu = nn.LeakyReLU(negative_slope=0.2, inplace=True)

    def forward(self, x):
        x1 = self.lrelu(self.conv1(x))
        x2 = self.lrelu(self.conv2(torch.cat((x, x1), 1)))
        x3 = self.lrelu(self.conv3(torch.cat((x, x1, x2), 1)))
        x4 = self.lrelu(self.conv4(torch.cat((x, x1, x2, x3), 1)))
        x5 = self.conv5(torch.cat((x, x1, x2, x3, x4), 1))
        return x5 * 0.2 + x


class RRDB(nn.Module):
    def __init__(self, nf, gc=32):
        super().__init__()
        self.rdb1 = DenseBlock(nf, gc)
        self.rdb2 = DenseBlock(nf, gc)
        self.rdb3 = DenseBlock(nf, gc)

    def forward(self, x):
        out = self.rdb1(x)
        out = self.rdb2(out)
        out = self.rdb3(out)
        return out * 0.2 + x


class RealESRGAN(nn.Module):
    def __init__(self, n_in=3, n_out=3, nf=64, nb=23, gc=32, scale=4):
        super().__init__()
        self.conv_first = nn.Conv2d(n_in, nf, 3, 1, 1, bias=True)
        self.body = nn.Sequential(*[RRDB(nf, gc) for _ in range(nb)])
        self.conv_body = nn.Conv2d(nf, nf, 3, 1, 1, bias=True)
        # upsample: 4x = two interpolate 2x stages (Real-ESRGAN x4plus style)
        self.conv_up1 = nn.Conv2d(nf, nf, 3, 1, 1, bias=True)
        self.conv_up2 = nn.Conv2d(nf, nf, 3, 1, 1, bias=True)
        self.lrelu = nn.LeakyReLU(negative_slope=0.2, inplace=True)
        self.conv_hr = nn.Conv2d(nf, nf, 3, 1, 1, bias=True)
        self.conv_last = nn.Conv2d(nf, n_out, 3, 1, 1, bias=True)

    def forward(self, x):
        fea = self.conv_first(x)
        trunk = self.conv_body(self.body(fea))
        fea = fea + trunk
        fea = self.lrelu(F.interpolate(self.conv_up1(fea), scale_factor=2, mode='nearest'))
        fea = self.lrelu(F.interpolate(self.conv_up2(fea), scale_factor=2, mode='nearest'))
        out = self.conv_last(self.lrelu(self.conv_hr(fea)))
        return out


# ---------------------------------------------------------------------------
# Model loading
# ---------------------------------------------------------------------------
def load_model(model_path, device):
    model = RealESRGAN(scale=4).to(device)
    state = torch.load(model_path, map_location=device, weights_only=False)
    # Real-ESRGAN checkpoints may be under 'params', 'params_ema', or plain state_dict
    if isinstance(state, dict):
        for key in ('params_ema', 'params', 'state_dict'):
            if key in state:
                state = state[key]
                break
    model.load_state_dict(state, strict=True)
    model.eval()
    return model


# ---------------------------------------------------------------------------
# Tiled inference (avoids OOM on large images)
# ---------------------------------------------------------------------------
def upscale_tensor(model, img_tensor, device, tile=512, tile_pad=32, scale=4):
    """
    img_tensor: (1, 3, H, W) in [-1, 1]
    Returns upscaled (1, 3, H*scale, W*scale) in [-1, 1]
    """
    _, _, h, w = img_tensor.shape
    out_h, out_w = h * scale, w * scale
    output = torch.zeros((1, 3, out_h, out_w), device=device)

    # tile grid
    step = tile - tile_pad * 2
    for y in range(0, h, step):
        for x in range(0, w, step):
            y1 = max(0, y - tile_pad)
            x1 = max(0, x - tile_pad)
            y2 = min(h, y + step + tile_pad)
            x2 = min(w, x + step + tile_pad)

            tile_in = img_tensor[:, :, y1:y2, x1:x2]
            with torch.no_grad():
                tile_out = model(tile_in)

            # map back into output
            oy1 = (y1 - y) * scale if y > 0 else 0
            ox1 = (x1 - x) * scale if x > 0 else 0
            oy2 = oy1 + (y2 - y1) * scale
            ox2 = ox1 + (x2 - x1) * scale

            # skip padded border except at image edges
            sy1 = 0 if y == 0 else tile_pad * scale
            sx1 = 0 if x == 0 else tile_pad * scale
            sy2 = tile_out.shape[2] if y2 == h else tile_out.shape[2] - tile_pad * scale
            sx2 = tile_out.shape[3] if x2 == w else tile_out.shape[3] - tile_pad * scale

            output[:, :, oy1 + sy1:oy1 + sy2, ox1 + sx1:ox1 + sx2] = \
                tile_out[:, :, sy1:sy2, sx1:sx2]

    return output


def pil_to_tensor(img):
    # PIL RGB -> torch (1,3,H,W) in [-1,1]
    arr = np.array(img.convert('RGB'), dtype=np.float32) / 255.0
    arr = (arr - 0.5) / 0.5  # [-1, 1]
    t = torch.from_numpy(arr).permute(2, 0, 1).unsqueeze(0)
    return t


def tensor_to_pil(t):
    t = t.squeeze(0).permute(1, 2, 0).cpu().numpy()
    t = (t * 0.5 + 0.5) * 255.0
    t = np.clip(t, 0, 255).astype(np.uint8)
    return Image.fromarray(t, 'RGB')


# ---------------------------------------------------------------------------
# Main upscale routine
# ---------------------------------------------------------------------------
def process_one(args):
    src_path, dest_dir, model, device, scale, target_max = args
    try:
        img = Image.open(src_path).convert('RGB')
    except Exception as e:
        return (src_path, False, f'cannot open: {e}')

    w, h = img.size
    # For 2k/4k/8k we compute target size from max side
    ratio = target_max / max(w, h)
    new_w = max(1, round(w * ratio))
    new_h = max(1, round(h * ratio))

    # Always upscale with AI model first (4x), then resize to target max side.
    # This ensures even tiny source textures (e.g. 64x64) get AI-enhanced detail
    # before being resized to the final 2K/4K/8K target.
    t = pil_to_tensor(img).to(device)
    with torch.no_grad():
        up = upscale_tensor(model, t, device)
    up_img = tensor_to_pil(up)  # w*4 x h*4
    if max(up_img.size) != max(new_w, new_h):
        up_img = up_img.resize((new_w, new_h), Image.LANCZOS)
    out_img = up_img

    dest_path = os.path.join(dest_dir, os.path.basename(src_path))
    out_img.save(dest_path, 'PNG', compress_level=6)
    return (src_path, True, f'{w}x{h} -> {out_img.size[0]}x{out_img.size[1]}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, help='source folder with category subdirs')
    ap.add_argument('--dest', required=True, help='destination root (will create 2k/4k/8k)')
    ap.add_argument('--scales', nargs='+', type=int, default=[2, 4, 8],
                    help='target max side in K: 2=2048, 4=4096, 8=8192')
    ap.add_argument('--model', default=r'C:\Users\Trilex\AppData\Local\Temp\opencode\esrgan_models\RealESRGAN_x4plus.pth')
    ap.add_argument('--device', default='cuda' if torch.cuda.is_available() else 'cpu')
    ap.add_argument('--workers', type=int, default=2, help='parallel image workers')
    args = ap.parse_args()

    device = torch.device(args.device)
    print(f'Device: {device}')
    if device.type == 'cuda':
        print(f'GPU: {torch.cuda.get_device_name(0)}')
        print(f'VRAM: {torch.cuda.get_device_properties(0).total_memory / 1e9:.1f} GB')

    model = load_model(args.model, device)
    print('Model loaded.')

    source = Path(args.source)
    dest = Path(args.dest)

    # discover categories
    cats = [d.name for d in source.iterdir() if d.is_dir()]
    print(f'Categories: {cats}')

    total_done = 0
    for k in args.scales:
        target_max = k * 1024
        print(f'\n=== Upscaling to {k}K (max side {target_max}px) ===')
        for cat in cats:
            cat_src = source / cat
            if not cat_src.is_dir():
                continue
            files = [f for f in cat_src.iterdir() if f.suffix.lower() == '.png']
            if not files:
                continue

            cat_dest = dest / f'{k}k' / cat
            cat_dest.mkdir(parents=True, exist_ok=True)

            tasks = [(str(f), str(cat_dest), model, device, 4, target_max) for f in files]
            ok = fail = 0
            with ThreadPoolExecutor(max_workers=args.workers) as ex:
                futs = [ex.submit(process_one, t) for t in tasks]
                for fut in as_completed(futs):
                    src, success, msg = fut.result()
                    if success:
                        ok += 1
                    else:
                        fail += 1
                        print(f'  FAIL {os.path.basename(src)}: {msg}')
            total_done += ok
            print(f'  {cat}: {ok} done, {fail} failed -> {cat_dest}')

    print(f'\nTotal upscaled: {total_done} images')
    print(f'Output root: {dest}')


if __name__ == '__main__':
    main()
