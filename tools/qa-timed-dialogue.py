"""Offline ASR evidence for original natural takes and decoded native audio."""
import argparse
import json
import os
from pathlib import Path
import sys
import wave

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--pocket-tools', required=True)
a = p.parse_args()
sys.path.insert(0, a.pocket_tools)
os.environ['HF_HUB_OFFLINE'] = '1'
import numpy as np
from faster_whisper import WhisperModel
model = WhisperModel('small', device='cpu', compute_type='int8', local_files_only=True)
manifest = json.loads((a.package / 'manifest.json').read_text(encoding='utf-8'))
results = []
for line in manifest['lines']:
    path = a.package / 'encoded' / (line['key'] + '.decoded.wav')
    with wave.open(str(path)) as w:
        rate = w.getframerate()
        assert w.getnchannels() == 1
        x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float32) / 32768
    x = np.interp(np.arange(round(len(x) * 16000 / rate)) * rate / 16000, np.arange(len(x)), x).astype(np.float32)
    segments, _ = model.transcribe(x, language='pl', beam_size=5, condition_on_previous_text=False)
    text = ' '.join(s.text.strip() for s in segments)
    results.append(dict(key=line['key'], expected=line['text_pl'], decoded_asr=text))
    print(line['key'], repr(text), flush=True)
(a.package / 'asr.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
