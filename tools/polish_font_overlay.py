"""Repair official DLC font overrides using verified native base font output.

All asset inputs/output remain private. Never discovers or installs into Steam.
"""
import argparse
import bisect
import json
from pathlib import Path
import shutil
import struct

from polish_fonts import FONTS, PL_LETTERS
from polish_pack import Cache, require, sha
from polish_release import payload, verify_archives, json_write
from polish_package_install import BASE_FONT_NAMES, FONT_OVERLAY_NAMES


def overlay_header(original, repaired, override, offset):
    # The official Corbel_26 override carries additional trailing metadata.
    # Apply only the two proven count changes; retain that metadata verbatim.
    allowed=set(range(offset,offset+4)) | set(range(offset+32,offset+36))
    require(len(original)==len(repaired), 'Base header size changed')
    require(all(a==b or i in allowed for i,(a,b) in enumerate(zip(original,repaired))),
            'Unexpected base header mutation')
    require(override[:offset+64]==original[:offset+64], 'DLC font header contract differs')
    result=bytearray(override)
    for off in (offset,offset+32):result[off:off+4]=repaired[off:off+4]
    require(all(a==b or i in allowed for i,(a,b) in enumerate(zip(override,result))),
            'DLC metadata changed')
    return bytes(result)


def check_lookup(header, body, offset, original_codes):
    n=struct.unpack_from('<I',header,offset)[0]
    codes=struct.unpack_from('<'+str(n)+'H',body,n*44)
    require(all(a<b for a,b in zip(codes,codes[1:])), 'Native lower_bound requires sorted codes')
    # 00524B90/006F6370 use an unsigned UTF-16 lower_bound over this array.
    fallback=struct.unpack_from('<I',header,offset+96)[0]
    require(codes[fallback]==ord('*'),'Default glyph index changed')
    for code in list(original_codes)+[x[1] for x in PL_LETTERS]:
        i=bisect.bisect_left(codes,code)
        require(i<n and codes[i]==code and (code==ord('*') or i!=fallback),
                'Native glyph lookup would fall back: '+hex(code))
    return n


def build(stage,base_stage,out,quickbms=None,bms_script=None):
    require(not out.exists(),'Output package already exists')
    replacements={stem:{} for stem in ('B.Font','H.Font')};rows=[]
    caches={stem:Cache(stage/'baseline',stem) for stem in replacements}
    require(caches['B.Font'].entries.keys()==caches['H.Font'].entries.keys(),'DLC paired inventory differs')
    for name,_,offset,count,*_ in FONTS:
        if name not in caches['B.Font'].entries:continue
        old=(stage/'extract/B.Font'/name).read_bytes()
        original=(base_stage/'extract/B.Font'/name).read_bytes()
        require(old==original,'Official DLC bulk differs; must reconcile: '+name)
        before=(base_stage/'extract/H.Font'/name).read_bytes()
        dlc_header=(stage/'extract/H.Font'/name).read_bytes()
        new_header=overlay_header(before,payload(base_stage/'candidate','H.Font',name),dlc_header,offset)
        new_bulk=payload(base_stage/'candidate','B.Font',name)
        codes=struct.unpack_from('<'+str(count)+'H',old,count*44)
        n=check_lookup(new_header,new_bulk,offset,codes)
        replacements['B.Font'][name]=new_bulk;replacements['H.Font'][name]=new_header
        rows.append(dict(font=name,glyphs=n,polish_glyphs=18,lookup='PASS',official_metadata='PRESERVED'))
    expected={name for name in caches['B.Font'].entries if name.startswith('Temp/FontCache/') and not name.endswith('_hjp.syn') and
              (stage/'extract/B.Font'/name).stat().st_size>16}
    require(set(replacements['B.Font'])==expected,'Unprocessed live DLC font')
    require(rows,'No official font overrides found')
    for stem,c in caches.items():c.write(stage/'candidate',replacements[stem])
    archives=verify_archives(stage,quickbms,bms_script)
    out.mkdir(parents=True);(out/'payload').mkdir()
    for name in sorted(FONT_OVERLAY_NAMES):shutil.copyfile(stage/'candidate'/name,out/'payload'/name)
    manifest=dict(schema=1,kind='dlc-font-overlay-v1',files={n:sha(out/'payload'/n) for n in sorted(FONT_OVERLAY_NAMES)},
                  preimage_profiles={'official':{n:sha(stage/'baseline'/n) for n in sorted(FONT_OVERLAY_NAMES)}},
                  base_font_files={n:sha(base_stage/'candidate'/n) for n in sorted(BASE_FONT_NAMES)},
                  fonts=rows,archives=archives,runtime='NOT_RUN; Polish glyph acceptance pending')
    json_write(out/'manifest.json',manifest)
    for name in ('polish_package_install.py','polish_pack.py'):
        shutil.copyfile(Path(__file__).parent/name,out/name)
    (out/'README.txt').write_text('Install only with the matching corrected base font archives.\n'
        'python polish_package_install.py install --package <package> --cache <DLC cache> --base-cache <base cache> --backup <new backup>\n'
        'Rollback: python polish_package_install.py rollback --cache <DLC cache> --backup <backup>\n'
        'This package changes four DLC font cache/TOC files; no DLL, INI, audio or translations.\n',encoding='utf8')
    (out/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.relative_to(out).as_posix()+'\n'
        for p in sorted(out.rglob('*')) if p.is_file()),encoding='utf8')
    return manifest


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ('stage','base-stage','out'):p.add_argument('--'+arg,type=Path,required=True)
    p.add_argument('--quickbms',type=Path);p.add_argument('--bms-script',type=Path)
    a=p.parse_args();m=build(a.stage,a.base_stage,a.out,a.quickbms,a.bms_script)
    print('DLC_FONT_OVERLAYS_PASS',len(m['fonts']))
