"""Offline, resumable native-category translation. Private inputs/outputs only."""
from __future__ import annotations
import argparse
import ast
import json
import re
from pathlib import Path
from collections import Counter
from polish_pack import category, require, sha, translate
from polish_ui_reviewed import REVIEWED

# Split around native syntax rather than asking a language model to reproduce it.
TOKEN = re.compile(r'<[^>]+>|%(?:\d+\$)?[-+0 #]*\d*(?:\.\d+)?(?:[sdifu]|ll[du])|%%|%\d+|\{[^{}]+\}|\[[A-Z0-9_]+\]|[\r\n]+')
REVIEWED_SEGMENTS={'Earn 20% extra XP when eating a heart':'Za zjedzenie serca otrzymujesz o 20% więcej XP'}

def segments(text):
    result=[];pos=0
    for match in TOKEN.finditer(text):
        result.extend(((False,text[pos:match.start()]),(True,match.group())))
        pos=match.end()
    result.append((False,text[pos:]));return result

def translatable(piece):
    return bool(re.search(r'[A-Za-z]{2}',piece)) and not re.fullmatch(r'[\s\d\W_]+',piece)

def assemble(text, cache):
    pieces=[]
    for protected,piece in segments(text):
        stripped=piece.strip()
        if protected or not translatable(stripped):pieces.append(piece);continue
        require(stripped in cache,'Missing translation segment')
        replacement=cache[stripped]
        pieces.append(piece[:len(piece)-len(piece.lstrip())]+replacement+piece[len(piece.rstrip()):])
    result=''.join(pieces)
    require(Counter(TOKEN.findall(text))==Counter(TOKEN.findall(result)),'Native placeholders changed')
    return result

def build(stage, story_manifest, menu_source, model_root, dependency_dirs, batch=8, threads=2, int8=False):
    import sys,os
    for path in reversed(dependency_dirs):sys.path.insert(0,str(path))
    os.environ['HF_HUB_OFFLINE']='1';os.environ['TRANSFORMERS_OFFLINE']='1'
    source=next((stage/'extract/H.Misc').rglob('Languages.cl')).read_bytes()
    markers=list(dict.fromkeys(re.findall(rb'/D2/Language/[A-Za-z0-9_]+/',source)))
    tree=ast.parse(menu_source.read_text(encoding='utf-8-sig'))
    menu=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
              and any(isinstance(t,ast.Name) and t.id=='MENU_TRANSLATIONS' for t in n.targets))
    story={item['key'].casefold():item.get('text_pl') or item.get('text_en','')
           for item in map(json.loads,story_manifest.read_text(encoding='utf-8-sig').splitlines())}
    approved={'Menu':menu,'SPStoryDialog':story}
    categories={};pending=set()
    for marker in markers:
        name=marker.decode().split('/')[-2];entries=category(source,marker)[3];mapping={}
        for key,text,_ in entries:
            corrected=REVIEWED.get(name,{}).get(key)
            if corrected is not None:
                require(Counter(TOKEN.findall(corrected))==Counter(TOKEN.findall(text)),'Reviewed UI placeholders changed: '+key)
                mapping[key]=corrected;continue
            saved=approved.get(name,{}).get(key.casefold() if name=='SPStoryDialog' else key)
            if saved and Counter(TOKEN.findall(saved))==Counter(TOKEN.findall(text)):
                mapping[key]=saved;continue
            # Credits contain attributions/names and must remain verbatim.
            if name=='Credits' or not text.strip():mapping[key]=text;continue
            for protected,piece in segments(text):
                if not protected and translatable(piece.strip()):pending.add(piece.strip())
        categories[name]=(marker,entries,mapping)
    cache_path=stage/'translation-segments.json'
    cache=json.loads(cache_path.read_text(encoding='utf8')) if cache_path.exists() else {}
    cache.update(REVIEWED_SEGMENTS)
    pending=sorted(pending-cache.keys(),key=lambda s:(len(s),s))
    print('Offline translation:',len(pending),'unique segments;',len(categories),'categories',flush=True)
    if pending:
        import torch
        from transformers import AutoTokenizer,AutoModelForSeq2SeqLM
        torch.set_num_threads(threads);torch.manual_seed(0)
        tok=AutoTokenizer.from_pretrained(str(model_root),src_lang='eng_Latn',local_files_only=True)
        model=AutoModelForSeq2SeqLM.from_pretrained(str(model_root),local_files_only=True).eval()
        if int8:
            model=torch.ao.quantization.quantize_dynamic(model,{torch.nn.Linear},dtype=torch.qint8)
        target=tok.convert_tokens_to_ids('pol_Latn')
        for start in range(0,len(pending),batch):
            group=pending[start:start+batch]
            inputs=tok(group,return_tensors='pt',padding=True,truncation=False)
            require(inputs['input_ids'].shape[1]<=512,'Translation segment exceeds model context; split explicitly')
            with torch.inference_mode():
                ids=model.generate(**inputs,forced_bos_token_id=target,max_new_tokens=512,num_beams=1,do_sample=False)
            for text,output in zip(group,tok.batch_decode(ids,skip_special_tokens=True)):
                require(output.strip() and '\x00' not in output,'Empty/unsafe model output')
                cache[text]=output.strip()
            checkpoint=cache_path.with_suffix('.tmp')
            checkpoint.write_text(json.dumps(cache,ensure_ascii=False,sort_keys=True),encoding='utf8')
            checkpoint.replace(cache_path)
            print('Translated',min(start+batch,len(pending)),'/',len(pending),flush=True)
    output=source;coverage=[];maps={}
    for name,(marker,entries,mapping) in categories.items():
        for key,text,_ in entries:
            if key not in mapping:mapping[key]=assemble(text,cache)
        output,report=translate(output,marker,mapping)
        require(not report['retained_for_tokens'],'Native printf/tag mismatch in '+name)
        maps[name]=mapping
        coverage.append(dict(category=name,entries=len(entries),changed=report['replaced'],
                             policy='attributions retained' if name=='Credits' else 'approved Polish + offline NLLB; linguistic QA pending'))
    (stage/'translations.json').write_text(json.dumps(maps,ensure_ascii=False,sort_keys=True,indent=2),encoding='utf8')
    (stage/'translation-coverage.json').write_text(json.dumps(coverage,ensure_ascii=False,indent=2),encoding='utf8')
    from polish_pack import Cache
    cache_archive=Cache(stage/'baseline','H.Misc')
    identity=next(p for p in cache_archive.entries if Path(p).name=='Languages.cl')
    cache_archive.write(stage/'candidate',{identity:output})
    return coverage

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','story-manifest','menu-source','model-root'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--dependency-dir',type=Path,action='append',default=[])
    p.add_argument('--batch',type=int,default=8)
    p.add_argument('--threads',type=int,default=2)
    p.add_argument('--int8',action='store_true',help='CPU dynamic quantization; generated strings still require language QA')
    a=p.parse_args();build(a.stage,a.story_manifest,a.menu_source,a.model_root,a.dependency_dir,a.batch,a.threads,a.int8)
