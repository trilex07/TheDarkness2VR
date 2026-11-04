"""Audit native shot consumer CFG; publish offsets/digests only, no game bytes."""
import argparse, hashlib
from pathlib import Path
import pefile
import native_view_clone_plan as cfg

p=argparse.ArgumentParser();p.add_argument('--exe',required=True,type=Path)
p.add_argument('--header',required=True,type=Path);a=p.parse_args()
assert hashlib.sha256(a.exe.read_bytes()).hexdigest()==cfg.EXE_SHA
pe=pefile.PE(str(a.exe));base=0xC47570;size=0x35D
cfg.FUNCTIONS=[(base,size)];r=cfg.audit(pe,base,size)
assert r['sha256']=='223bfc40a7bf09bb59b9d594802aceedff4a04b3c753978612cab69da80660e2'
assert (0xC47743-base,0xB27AA0) in r['calls']
spread=pe.get_data(0xB27AA0-0x400000,0x1B9)
text=['#pragma once','#include "native_view_code_clone.h"','namespace d2vr_native_clone {',
 'inline const Block twoHandConsumer{reinterpret_cast<void*>(0xC47570),0x35D,"'+r['sha256']+'"};',
 'inline const char* twoHandSpreadDigest="'+hashlib.sha256(spread).hexdigest()+'";',
 'inline const Call twoHandConsumerCalls[]={']
text += ['{0,0x%X,0x%X},'%c for c in r['calls']]
text += ['};','}']
a.header.write_text('\n'.join(text)+'\n')
print('PASS consumer CFG, complete call relocation, pinned spread, single C47743 replacement')
