"""Generate reviewed shorter lines into a private staging folder, never Steam.

Reject speech exceeding its native scene budget; no time stretching or speech
truncation. Trim only quiet exterior samples, with 30ms lead and 80ms tail.
Original voice donors and the existing local Polish model are reused offline.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import wave


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rewrites', type=Path, required=True)
    p.add_argument('--audit', type=Path, required=True)
    p.add_argument('--references', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--pocket-tools', type=Path, required=True)
    a = p.parse_args()
    os.environ['HF_HUB_OFFLINE'] = '1'
    sys.path.insert(0, str(a.pocket_tools))
    import numpy as np
    import torch
    from pocket_tts import TTSModel
    torch.set_num_threads(8)
    budgets = {e['key']: e for e in json.loads(a.audit.read_text(encoding='utf-8'))}
    rewrites = json.loads(a.rewrites.read_text(encoding='utf-8'))
    a.out.mkdir(parents=True, exist_ok=True)
    model = TTSModel.load_model(config='hf://shefowl/pocket-tts-polish-6l/config.yaml', temp=.65, quantize=True)
    sr = model.sample_rate
    reports = []
    for key, alternatives in rewrites.items():
        budget = budgets[key]['budget_seconds']
        donor = a.references / (key + '.wav')
        state = model.get_state_for_audio_prompt(str(donor))
        accepted = False
        for text in alternatives:
            for attempt in range(2):
                x = model.generate_audio(state, text).detach().cpu().numpy().squeeze()
                peak = float(np.max(np.abs(x)))
                if not peak > .001:
                    continue
                active = np.flatnonzero(np.abs(x) >= max(.001, peak * .004))
                start = max(0, int(active[0]) - int(sr * .03))
                end = min(len(x), int(active[-1]) + int(sr * .08) + 1)
                # Require actual generated quiet tail: never call a word cut at
                # the end of generation a successfully shortened utterance.
                quiet_tail = (len(x) - int(active[-1]) - 1) / sr
                y = x[start:end]
                seconds = len(y) / sr
                print(key, repr(text), round(seconds, 3), 'budget', round(budget, 3), 'tail', round(quiet_tail, 3), flush=True)
                if seconds > budget - .02 or quiet_tail < .08:
                    continue
                y = np.clip(y * min(1., .89 / peak), -1, 1)
                with wave.open(str(a.out / (key + '.wav')), 'wb') as w:
                    w.setparams((1, 2, sr, 0, 'NONE', 'not compressed'))
                    w.writeframes((y * 32767).astype('<i2').tobytes())
                reports.append(dict(key=key, text_pl=text, seconds=seconds, budget_seconds=budget, generated_tail_seconds=quiet_tail, tempo=1.0, path=budgets[key]['path']))
                accepted = True
                break
            if accepted:
                break
        if not accepted:
            reports.append(dict(key=key, status='REWRITE_REQUIRED', budget_seconds=budget))
        (a.out / 'report.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
    if any(e.get('status') for e in reports):
        raise SystemExit('Some lines require shorter text; no acceleration/truncation permitted')


if __name__ == '__main__':
    main()
