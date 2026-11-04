"""Build private, independent voice/text profiles from verified local caches.

Original English Languages.cl must be selectively extracted before invoking.
Never downloads or publishes game assets and never modifies installed files.
"""
import argparse
import json
from pathlib import Path
import shutil
from polish_pack import Cache, category, require, sha


def build(installed, vanilla, english_text, output):
    require(not output.exists(), 'Fresh profile output required')
    raw = english_text.read_bytes()
    _, _, _, entries = category(raw, b'/D2/Language/Menu/')
    require(dict((k, t) for k, t, _ in entries)['Shared_Back'] == '<MENU_CANCEL>Back',
            'Original English text not established')
    names = [s + e for s in ('H.Misc', 'H.Misc_en', 'B.Misc_en', 'F.Misc_en')
             for e in ('.cache', '.toc')]
    manifest = {'en': {}, 'pl': {}, 'provenance': {}}
    for lang in ('en', 'pl'):
        folder = output / 'languages' / lang
        folder.mkdir(parents=True)
        if lang == 'en':
            Cache(installed, 'H.Misc').write(folder, {'Languages.cl': raw})
        for name in names:
            if lang == 'en' and name.startswith('H.Misc.'):
                pass
            else:
                source = (vanilla if lang == 'en' else installed) / name
                shutil.copyfile(source, folder / name)
                manifest['provenance'][str(source.resolve())] = sha(source)
            manifest[lang][name] = sha(folder / name)
    # Original English voices have matching header/bulk archive identities;
    # unrelated assets in H.Misc retain the current installed TOC records.
    manifest['provenance'][str(english_text.resolve())] = sha(english_text)
    (output / 'languages.json').write_text(json.dumps(manifest, indent=2))
    shutil.copyfile(Path(__file__).with_name('Apply-Languages.ps1'),
                    output / 'Apply-Languages.ps1')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('installed', 'vanilla', 'english-text', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    build(a.installed, a.vanilla, a.english_text, a.output)
    print('PROFILES_VERIFIED: independent text and voice, 16 payload hashes')
