"""Shared cache of REA decompile/assembly results (2026-10-23, user request).

REA's Ghidra import is ephemeral: every new REA session re-analyses the whole
EXE (minutes). Look here and in ghidra_full first; missing original-game
functions use the persistent read-only DB, never a new REA import. Store
results only from analysis allowed by the session rules. Entries are keyed by the analysed binary's
SHA-256, so results of another EXE build are never mixed in.

  python tools/decomp_cache.py get 0x85FCE0        (also 0x14085FCE0; exit 1 if missing)
  python tools/decomp_cache.py list [digest-prefix]
  python tools/decomp_cache.py harvest [jsonl...]   (REA results in Claude transcripts;
                                                     default: this project's transcripts)
  python tools/decomp_cache.py import result.json   (one saved REA tool result)
  python tools/decomp_cache.py put 0x85FCE0 --kind c --file text.c [--digest sha]
"""
from pathlib import Path
import argparse,hashlib,json,os,sys,time
P=Path(__file__).resolve().parents[1]
CACHE=P/'research/decomp_cache'
import ghidra_full
DEFAULT_DIGEST=ghidra_full.BUILDS[ghidra_full.DEFAULT_BUILD][1]   # follows ghidra_full.DEFAULT_BUILD
TRANSCRIPTS=Path.home()/'.claude/projects/C--Program-Files--x86--Steam-steamapps-common-Romance-of-the-Three-Kingdoms-14'
KINDS={'procedure_pseudo_code':'c','procedure_assembly':'asm','batch_decompile':'c'}

def rva(text):
    v=int(str(text),16)
    return v-0x140000000 if v>=0x140000000 else v
def path(digest,address,kind):return CACHE/digest[:16]/('%08X_%s.txt'%(address,kind))
def store(digest,address,kind,text,source):
    if not text or not text.strip():return False
    text=text.replace('\r\n','\n').strip('\n')+'\n'
    f=path(digest,address,kind);f.parent.mkdir(parents=True,exist_ok=True)
    if f.exists() and f.read_text(encoding='utf-8').split('\n',3)[-1]==text:return False
    head='# rva=%08X kind=%s digest=%s\n# source=%s\n# saved=%s\n'%(address,kind,digest,source,time.strftime('%Y-%m-%d %H:%M'))
    f.write_text(head+text,encoding='utf-8',newline='\n');return True
def digests():
    return sorted(d.name for d in CACHE.iterdir() if d.is_dir()) if CACHE.exists() else []

def from_evidence(e,source):
    """Store every function of one REA evidence object; returns stored count."""
    op=e.get('operation');kind=KINDS.get(op)
    digest=(((e.get('subject') or {}).get('digest') or {}).get('sha256'))
    if not kind or not digest:return 0
    out=e.get('normalized_result');n=0
    if op=='batch_decompile':
        for item in (out or {}).get('items',[]):
            if item.get('status')=='ok':n+=store(digest,rva(item['address']),'c',item.get('pseudocode') or '',source)
    elif isinstance(out,str):
        n+=store(digest,rva((e.get('parameters') or {}).get('procedure','0')),kind,out,source)
    return n
def evidence_texts(value):
    # Tool results: a string, or a list of {"type":"text","text":...} blocks.
    if isinstance(value,str):
        if value.lstrip().startswith('{"evidence_id"'):yield value
    elif isinstance(value,list):
        for v in value:yield from evidence_texts(v.get('text') if isinstance(v,dict) else v)
    elif isinstance(value,dict):
        if 'content' in value:yield from evidence_texts(value['content'])
def harvest(files):
    total=0
    for f in files:
        try:lines=open(f,encoding='utf-8',errors='replace')
        except OSError:continue
        for line in lines:
            if '"evidence_id' not in line:continue
            try:record=json.loads(line)
            except ValueError:continue
            content=((record.get('message') or {}).get('content'))
            if not isinstance(content,list):continue
            for block in content:
                if not isinstance(block,dict) or block.get('type')!='tool_result':continue
                for text in evidence_texts(block.get('content')):
                    try:total+=from_evidence(json.loads(text),Path(f).name)
                    except (ValueError,KeyError,TypeError):pass
    return total

def main():
    ap=argparse.ArgumentParser();sub=ap.add_subparsers(dest='cmd',required=True)
    g=sub.add_parser('get');g.add_argument('address');g.add_argument('--digest',default=DEFAULT_DIGEST)
    l=sub.add_parser('list');l.add_argument('prefix',nargs='?',default=DEFAULT_DIGEST[:16])
    h=sub.add_parser('harvest');h.add_argument('files',nargs='*')
    i=sub.add_parser('import');i.add_argument('file')
    p=sub.add_parser('put');p.add_argument('address');p.add_argument('--kind',choices=('c','asm'),required=True)
    p.add_argument('--file',required=True);p.add_argument('--digest',default=DEFAULT_DIGEST)
    a=ap.parse_args()
    if a.cmd=='get':
        address=rva(a.address);found=False
        for kind in ('c','asm'):
            f=path(a.digest,address,kind)
            if f.exists():found=True;print(f.read_text(encoding='utf-8'));print()
        bld=next((k for k,v in ghidra_full.BUILDS.items() if v[1]==a.digest.lower()),None)
        if not found and bld:
            root=ghidra_full.Root(ghidra_full.build_dir(bld),bld)
            try:f=root.find_func(a.address)
            except OSError:f=None
            if f is not None:
                print('# source=ghidra_full build=%s'%bld)
                print('\n'.join(root.body(f)))
                found=True
        if not found:
            other=[d for d in digests() if d!=a.digest[:16] and any((CACHE/d).glob('%08X_*'%address))]
            print('missing %08X for %s'%(address,a.digest[:16])+(' (other binaries: %s)'%', '.join(other) if other else ''))
            sys.exit(1)
    elif a.cmd=='list':
        d=CACHE/a.prefix[:16]
        for f in sorted(d.glob('*.txt')) if d.exists() else []:print(f.stem)
    elif a.cmd=='harvest':
        files=a.files or sorted(str(f) for f in TRANSCRIPTS.glob('**/*.jsonl'))
        print(json.dumps(dict(files=len(files),stored=harvest(files)),ensure_ascii=False))
    elif a.cmd=='import':
        text=Path(a.file).read_text(encoding='utf-8');n=0
        for t in list(evidence_texts(json.loads(text))) or [text]:n+=from_evidence(json.loads(t),Path(a.file).name)
        print(json.dumps(dict(stored=n)))
    elif a.cmd=='put':
        print(json.dumps(dict(stored=store(a.digest,rva(a.address),a.kind,Path(a.file).read_text(encoding='utf-8'),'manual'))))
if __name__=='__main__':main()
