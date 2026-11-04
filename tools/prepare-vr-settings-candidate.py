"""Freeze the MAIN-owned local VR settings/font candidate with full rollback."""
import json
import shutil
import subprocess
from pathlib import Path
from polish_pack import require,sha

ROOT=Path(__file__).resolve().parents[1]
def main():
    game=Path('C:/Program Files (x86)/Steam/steamapps/common/Darkness II')
    lab=ROOT/'test-output/vr-settings-menu-v2-20261104'
    evidence=ROOT/'test-output/vr-settings-fonts-20261104'
    stage=ROOT/'build/vr-settings-20261104'
    out=ROOT/'build/vr-settings-fonts-candidate-20261104'
    require(not out.exists(),'Fresh frozen output required')
    require(not subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=ROOT).strip(),'Commit source first')
    source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    out.mkdir();files=[]
    def add(relative,after,before=None,allowed=()):
        target=out/'payload'/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(after,target)
        old=None
        if before:
            backup=out/'rollback'/relative;backup.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(before,backup);old=sha(backup)
        files.append(dict(relative=relative,after=sha(target),before=old,allowed=list(allowed)))
    main_ini=stage/'candidate-main.ini'
    baseline_ini=lab/'rollback/Darkness2VR.ini'
    text=baseline_ini.read_text(encoding='ascii')
    require('[VRSettings]' not in text,'Unexpected original settings section')
    main_ini.write_text(text+'\n[VRSettings]\nNIS=1\nNISPercent=100\nSupersampling=100\nPolishVoice=1\nPolishText=1\nAppliedPolishVoice=1\nAppliedPolishText=1\nLanguageError=0\n',encoding='ascii')
    add('d3d9.dll',stage/'diagnostic.dll',lab/'rollback/d3d9.dll')
    add('Darkness2VR.ini',main_ini,baseline_ini)
    add('Darkness2VR-diagnostic.ini',lab/'rollback/Darkness2VR-diagnostic.ini',lab/'rollback/Darkness2VR-diagnostic.ini')
    for stem in ('H.Font','B.Font'):
        for ext in ('.cache','.toc'):
            n=stem+ext;add('Cache.DLC/PCPatchManifests/Cache.Windows/'+n,
                stage/'dlc-font-sync/payload'/n,evidence/'dlc-input'/n)
    profiles=stage/'VRSettings';manifest=json.loads((profiles/'languages.json').read_text())
    for n in manifest['pl']:
        add('Cache.Windows/'+n,evidence/'installed'/n,evidence/'installed'/n,[manifest['en'][n]])
    for n in ('Apply-Languages.ps1','languages.json'):add('VRSettings/'+n,profiles/n)
    for lang in ('en','pl'):
        for n,h in manifest[lang].items():
            require(sha(profiles/'languages'/lang/n)==h,'Language payload changed')
            add('VRSettings/languages/'+lang+'/'+n,profiles/'languages'/lang/n)
    protected=[]
    for relative in ['DarknessII.exe','openxr_loader.dll']+[
        'Cache.Windows/'+s+e for s in ('H.Font','B.Font','H.Texture','B.Texture','F.Texture','H.Texture_en','B.Texture_en','H.AnimRetarget','B.AnimRetarget','H.Script','B.Script')for e in ('.cache','.toc')]:
        protected.append(dict(relative=relative,hash=sha(game/relative)))
    (out/'candidate.json').write_text(json.dumps(dict(game=str(game),source=source,files=files,protected=protected),indent=2))
    shutil.copyfile(ROOT/'tools/vr-settings-install.ps1',out/'Install.ps1')
    print('FROZEN',source,len(files),'targets',out)

if __name__=='__main__':main()
