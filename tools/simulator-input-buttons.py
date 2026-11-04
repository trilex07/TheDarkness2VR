"""Local test runtime only: add leased grip/stick clicks to existing UI IPC.

Does not change the installed runtime registry or production mod. Rebuild the
local x86 simulator with the game closed after applying. Existing capture,
focus, action-snapshot acknowledgement and release/expiry guards remain active.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]/'tools-local/OpenXR-Simulator'
changes={
 'src/ui_automation_runtime.h':[
  ('ctrl.triggerValue=0; ctrl.thumbstick={0,0};','ctrl.triggerValue=0; ctrl.thumbstick={0,0}; ctrl.gripPressed=ctrl.thumbstickPressed=false; ctrl.gripValue=0;'),
  ('incoming.button!="secondary" && incoming.button!="menu")','incoming.button!="secondary" && incoming.button!="menu" && incoming.button!="grip" && incoming.button!="stick")'),
  ('ctrl.gripPressed=ctrl.thumbstickPressed=false;\n        ctrl.gripValue=0; ctrl.thumbstick=ui_auto::PointerThumbstick(p);',
   'ctrl.gripPressed=p.down && p.button=="grip";\n        ctrl.thumbstickPressed=p.down && p.button=="stick";\n        ctrl.gripValue=ctrl.gripPressed?1.0f:0.0f; ctrl.thumbstick=ui_auto::PointerThumbstick(p);')],
 'src/action_sync.h':[
  ('(p.button=="menu" && path.find("/input/menu/click")!=std::string::npos) ||',
   '(p.button=="menu" && path.find("/input/menu/click")!=std::string::npos) ||\n                           (p.button=="stick" && path.find("/input/thumbstick/click")!=std::string::npos) ||\n                           (p.button=="grip" && path.find("/input/squeeze/")!=std::string::npos) ||')],
 'mcp-server/ui_targeting.py':[
  ('("trigger", "primary", "secondary", "menu", "select")',
   '("trigger", "primary", "secondary", "menu", "select", "grip", "stick")')]
}
prepared=[]
for name,pairs in changes.items():
 p=root/name;text=p.read_text(encoding='utf-8');original=text
 for before,after in pairs:
  if after in text:continue
  if text.count(before)!=1:raise ValueError('Unexpected simulator source: '+name)
  text=text.replace(before,after)
 prepared.append((p,original,text))
for p,original,text in prepared:
 if original!=text:
  backup=p.with_suffix(p.suffix+'.before-menu-input')
  if backup.exists():raise ValueError('Backup exists: '+str(backup))
  backup.write_text(original,encoding='utf-8');p.write_text(text,encoding='utf-8')
print('Local simulator IPC buttons ready; production runtime unchanged')
