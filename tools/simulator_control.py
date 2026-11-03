"""Use the checked-out simulator's own controller/capture client, locally only."""
import argparse
import asyncio
import json
import sys
from pathlib import Path
from PIL import Image
from io import BytesIO

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools-local/OpenXR-Simulator/mcp-server"))
from ui_targeting import UiAutomation


async def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["capture", "click", "scroll"])
    p.add_argument("--button", default="primary")
    p.add_argument("--hand", choices=["left", "right"], default="right")
    p.add_argument("--x", type=float, default=.5)
    p.add_argument("--y", type=float, default=.5)
    p.add_argument("--stick-x", type=float, default=0)
    p.add_argument("--stick-y", type=float, default=0)
    p.add_argument("--frames", type=int, default=8)
    p.add_argument("--hold-seconds", type=float, default=0,
                   help="click: retain input for bounded wall time, then release")
    p.add_argument("--name", default="control")
    p.add_argument("--origin-mode", choices=["hand", "eye"], default="hand")
    p.add_argument("--lab", type=Path, default=ROOT / "test-output/simulator-lab-20261103")
    args = p.parse_args()
    if not 1 <= args.frames <= 90:
        p.error("--frames must be 1..90 xrSyncActions snapshots, not rendered frames")
    if not 0 <= args.hold_seconds <= 3 or (args.hold_seconds and args.action != "click"):
        p.error("--hold-seconds requires click and must be 0..3")
    if Path(args.name).name != args.name or args.name in (".", ".."):
        p.error("--name must be a file stem")
    lab = args.lab.resolve()
    ui = UiAutomation(lab / "runtime")
    info, pixels = await ui.capture("left", max_width=1000)
    result = info
    if args.action != "capture":
        kw = dict(hand=args.hand, button=args.button, hold_frames=args.frames,
                  origin_mode=args.origin_mode)
        if args.action == "scroll":
            kw["stick"] = [args.stick_x, args.stick_y]
        if args.hold_seconds:
            try:
                result, pixels = await ui.interact("hold", info["capture_id"],
                    args.x * info["image_size"][0], args.y * info["image_size"][1], **kw)
                await asyncio.sleep(args.hold_seconds)
            finally:
                result, pixels = await ui.interact("release", info["capture_id"], hand=args.hand)
        else:
            result, pixels = await ui.interact(args.action, info["capture_id"],
                args.x * info["image_size"][0], args.y * info["image_size"][1], **kw)
    Image.open(BytesIO(pixels)).save(lab / f"{args.name}.png")
    (lab / f"{args.name}.json").write_text(json.dumps(result, indent=2))
    print(json.dumps({"image": str(lab / f"{args.name}.png"), "result": result.get("status", "captured")}))


if __name__ == "__main__":
    asyncio.run(main())
