"""Export a private, asset-keyed SIDE backlog from the verified deployed pack.

Does not modify Steam or generate speech. English source strings and voice
manifests belong outside Git, alongside the user's private localization inputs.
"""
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import re
import struct
import wave

from polish_pack import Cache, audio_format, category, require, sha
from polish_fonts import FONTS


def write_csv(path, fields, rows):
    with path.open('w', newline='', encoding='utf-8-sig') as stream:
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(rows)


def export(stage, game_cache, voices, manifest_path, out):
    report = json.loads((stage/'build-report.json').read_text(encoding='utf8'))
    require(not out.exists(), 'Output already exists; retain earlier backlog')
    for name, expected in report['files'].items():
        require(sha(game_cache/name) == expected, 'Deployed pack differs: '+name)
    manifest = [json.loads(line) for line in manifest_path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    by_key = {item['key']: item for item in manifest}
    original = next((stage/'extract/H.Misc').rglob('Languages.cl')).read_bytes()
    active = Cache(game_cache, 'H.Misc')
    language_path = [p for p in active.entries if Path(p).name == 'Languages.cl']
    require(len(language_path) == 1, 'Ambiguous live Languages.cl')
    _, offset, packed, size = active.entries[language_path[0]]
    require(packed == size, 'Live language payload is compressed; extract first')
    with (game_cache/'H.Misc.cache').open('rb') as stream:
        stream.seek(offset)
        current = stream.read(size)
    texts, coverage, source_by_key = [], [], {}
    for marker in dict.fromkeys(re.findall(rb'/D2/Language/[A-Za-z0-9_]+/', original)):
        _, _, _, entries = category(original, marker)
        _, _, _, deployed = category(current, marker)
        require([e[0] for e in entries] == [e[0] for e in deployed], 'Category identities changed')
        name = marker.decode().split('/')[-2]
        counts = Counter()
        for (key, source, _), (_, text, _) in zip(entries, deployed):
            source_by_key.setdefault(key.casefold(), []).append((name, source, text))
            status = 'CHANGED_REQUIRES_LANGUAGE_QA' if source != text else 'UNCHANGED_REVIEW_FOR_TRANSLATION'
            if not source.strip():
                status = 'EMPTY_SOURCE'
            counts[status] += 1
            if status != 'EMPTY_SOURCE':
                texts.append(dict(category=name, key=key, status=status, text_en=source,
                    deployed_text=text, proposed_pl=by_key.get(key, {}).get('text_pl', ''),
                    task='Sprawdz PL, zachowaj znaczniki i zmienne' if source != text else
                         'Przetlumacz; nazwy wlasne/liczby/etykiety techniczne zatwierdz recznie'))
        coverage.append(dict(category=name, entries=len(entries), **dict(counts)))
    h = Cache(stage/'baseline', 'H.Misc_en')
    b = Cache(stage/'baseline', 'B.Misc_en')
    f = Cache(stage/'baseline', 'F.Misc_en')
    rejected = {item['path']: item['reason'] for item in report['audio']['unsupported']}
    audio, states = [], Counter()
    matched = accepted = 0
    for path in sorted(h.entries):
        if not path.endswith('_en.wav') or path not in b.entries:
            continue
        key = Path(path).name.removesuffix('_en.wav')
        voice = voices/(key+'.wav')
        hp, bp = stage/'extract/H.Misc_en'/path, stage/'extract/B.Misc_en'/path
        available = voice.is_file() and hp.is_file() and bp.is_file()
        if available:
            matched += 1
        if available and path not in rejected:
            accepted += 1
            states['DEPLOYED_PL_REQUIRES_LISTENING_QA'] += 1
            continue
        reason = rejected.get(path, '')
        status = ('REPACK_SERIALIZATION_REQUIRED' if 'serialization' in reason else
                  'RETIME_EXISTING_PL_RECORDING' if reason else 'MISSING_PL_RECORDING')
        native_seconds = ''
        rate = channels = samples = ''
        if hp.is_file() and bp.is_file():
            try:
                stream_size = f.entries[path][3] if path in f.entries else None
                _, rate, channels, samples, *_ = audio_format(hp.read_bytes(), bp.read_bytes(),
                                                             stream_size, path not in f.entries)
                native_seconds = round(samples/rate, 6)
            except ValueError:
                pass
        generated_seconds = ''
        if voice.is_file():
            with wave.open(str(voice)) as stream:
                generated_seconds = round(stream.getnframes()/stream.getframerate(), 6)
        item = by_key.get(key, {})
        references = source_by_key.get(key.casefold(), [])
        unique_source = {r[1] for r in references}
        source_text = next(iter(unique_source)) if len(unique_source) == 1 else ''
        family = path.split('/')[2]
        priority = ('1_SP_STORY' if family == 'SPDialog' else
                    '2_SP_AMBIENT_OR_COMBAT_REVIEW' if family in
                    ('CombatBarks', 'SpCharacterBarks', 'SpLevelBarks', 'Barks', 'MissionDialog') else
                    '3_MP_VENDETTAS' if family in ('MPDialog', 'VendettasBarks') else
                    '4_REVIEW_LEGACY_DEMO_OR_UNKNOWN')
        audio.append(dict(priority=priority, key=key, native_path=path, status=status, reason=reason,
            storage='stream' if path in f.entries else 'resident',
            native_sample_rate=rate, native_channels=channels, native_samples=samples,
            native_seconds=native_seconds, existing_pl_seconds=generated_seconds,
            existing_pl_wav=str(voice) if voice.is_file() else '', speaker=item.get('speaker', ''),
            text_categories=';'.join(sorted({r[0] for r in references})),
            text_en=item.get('text_en', source_text), text_pl=item.get('text_pl', ''),
            task=('Dopasuj polska kwestie do czasu oryginalu; sprawdz odsluch i dekodowanie' if
                  status == 'RETIME_EXISTING_PL_RECORDING' else
                  'Napraw pakowanie istniejacego PL WAV; nie generuj ponownie bez potrzeby' if
                  status == 'REPACK_SERIALIZATION_REQUIRED' else
                  'Odsluchaj: zostaw oddechy/krzyki bez slow; kwestie mowione przetlumacz i nagraj w budzecie czasu. Brak tekstu = transkrypcja z oryginalu')))
        states[status] += 1
    require(matched == report['audio']['matched_live_assets'], 'Live audio matching differs from deployed build')
    require(accepted == report['audio']['replaced'], 'Audio acceptance differs from deployed build')
    fonts = [dict(font=path, status='ASCII_FALLBACK_ONLY', required_glyphs='ĄąĆćĘęŁłŃńÓóŚśŹźŻż',
        task='Ustal kodowanie atlasu, dodaj rzeczywiste glify, UV i metryki; zachowaj pozostale jezyki',
        header_count_offset=offset, original_english_glyphs=count) for path, offset, count in FONTS]
    audio.sort(key=lambda row: (row['priority'], row['native_path']))
    summary = dict(verified_pack_sha256=report['files'], text_categories=coverage,
        text_rows=len(texts), audio_statuses=dict(states), audio_backlog_rows=len(audio),
        generated_story_wavs=report['audio']['voice_files'], matched_live_story_assets=matched,
        deployed_audio_replacements=accepted, fonts_needing_real_polish_glyphs=len(fonts),
        limitations=['Unchanged text includes proper names, numbers and technical strings; review before translating.',
            'Live TOC assets include legacy/demo variants and nonverbal sounds; presence alone does not prove campaign playback or a dubbing requirement.',
            'Changed strings and deployed audio need linguistic/listening QA; differences do not prove Polish quality.',
            'Audio without manifest text needs transcription; do not guess dialogue from filenames.',
            'Native dialogue durations are format metadata, not a proof of scene timing or lip-sync.',
            'No game assets, English text or generated voice data should be committed to Git.'])
    out.mkdir(parents=True)
    write_csv(out/'texts.csv', list(texts[0]), texts)
    write_csv(out/'audio.csv', list(audio[0]), audio)
    write_csv(out/'fonts.csv', list(fonts[0]), fonts)
    (out/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(text_rows=len(texts), audio_statuses=dict(states), fonts=len(fonts)), ensure_ascii=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('stage', 'game-cache', 'voices', 'manifest', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    export(args.stage, args.game_cache, args.voices, args.manifest, args.out)
