"""Build six reviewed natural-speed villa lines from current installed archives.

Private audio/cache outputs stay outside Git. Does not modify the installation.
"""
import argparse
import json
from pathlib import Path
import struct
import wave
from polish_pack import Cache, audio_format, patch_xwma, command, require, sha, translate, category


def read(cache, path):
    _, off, packed, size = cache.entries[path]
    require(packed == size, 'Expected installed raw replacement: ' + path)
    with (cache.root / (cache.stem + '.cache')).open('rb') as f:
        f.seek(off)
        result = f.read(size)
    require(len(result) == size, 'Short cache read')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'voices', 'out', 'ffmpeg', 'encoder'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    require(not a.out.exists(), 'Fresh output required')
    report = json.loads((a.voices / 'report.json').read_text(encoding='utf-8'))
    keys = {'Sd02690Wiseguy1', 'Sd02700Wiseguy2', 'Sd02710Wiseguy1',
            'Sd02720Jackie', 'Sd02730Wiseguy1', 'Sd02740Wiseguy2'}
    require(len(report) == 6 and {r['key'] for r in report} == keys, 'Reviewed six-line set required')
    stems = ('H.Misc_en', 'B.Misc_en', 'F.Misc_en', 'H.Misc')
    caches = {s: Cache(a.cache, s) for s in stems}
    replacement = {s: {} for s in stems}
    before = {s + e: sha(a.cache / (s + e)) for s in stems for e in ('.cache', '.toc')}
    work = a.out / 'encoded'
    work.mkdir(parents=True)
    for r in report:
        require(r.get('tempo') == 1.0 and not r.get('status'), 'Unaccepted synthesis')
        path, key = r['path'], r['key']
        require(Path(path).name == key + '_en.wav', 'Unexpected line path')
        h, b = (read(caches[s], path) for s in stems[:2])
        size = caches['F.Misc_en'].entries[path][3]
        _, rate, channels, _, avg, _, count, _ = audio_format(h, b, size)
        require(count > 0, 'Only native streaming xWMA supported')
        voice = a.voices / (key + '.wav')
        with wave.open(str(voice)) as w:
            duration = w.getnframes() / w.getframerate()
        require(duration <= r['budget_seconds'] - .02, 'Text rewrite needed')
        samples = round(r['budget_seconds'] * rate)
        wav, encoded, decoded = [work / (key + ext) for ext in ('.wav', '.xwma', '.decoded.wav')]
        command([a.ffmpeg, '-v', 'error', '-y', '-i', voice, '-ar', rate, '-ac', channels,
                 '-af', f'aresample={rate},apad,atrim=end_sample={samples}', '-c:a', 'pcm_s16le', wav])
        command([a.encoder, '-b', avg * 8, wav, encoded])
        nh, nb, nf = patch_xwma(h, b, size, encoded.read_bytes())
        command([a.encoder, encoded, decoded])
        fmt = audio_format(nh, nb, len(nf))
        with wave.open(str(decoded)) as w:
            require((w.getframerate(), w.getnchannels(), w.getnframes()) == (rate, channels, fmt[3]), 'Codec roundtrip mismatch')
            require(w.getnframes() / rate >= duration, 'Codec truncated speech')
        for stem, data in zip(stems, (nh, nb, nf)):
            replacement[stem][path] = data
        r['voice_sha256'] = sha(voice)
        print(key, 'codec roundtrip PASS, tempo=1', flush=True)
    text_cache = caches['H.Misc']
    paths = [k for k in text_cache.entries if Path(k).name == 'Languages.cl']
    require(len(paths) == 1, 'Unique language resource required')
    old = read(text_cache, paths[0])
    marker = b'/D2/Language/SPStoryDialog/'
    translations = {r['key']: r['text_pl'] for r in report}
    new, stats = translate(old, marker, translations)
    original = {k: t for k, t, _ in category(old, marker)[3]}
    checked = {k: t for k, t, _ in category(new, marker)[3]}
    require(keys <= original.keys(), 'Missing subtitle key')
    require(all(checked[k] == translations.get(k, t) for k, t in original.items()), 'Subtitle readback mismatch')
    require(not stats['retained_for_tokens'], 'Subtitle token mismatch')
    replacement['H.Misc'][paths[0]] = new
    for s in stems:
        caches[s].write(a.out / 'payload', replacement[s])
    require(all(sha(a.cache / n) == h for n, h in before.items()), 'Source changed during build')
    files = {n: sha(a.out / 'payload' / n) for n in before}
    manifest = dict(kind='dialogue-natural-repair', before=before, files=files, lines=report, subtitle=stats)
    (a.out / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print('BUILD_VERIFIED', a.out, flush=True)


if __name__ == '__main__':
    main()
