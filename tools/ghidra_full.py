"""Query the read-only full Ghidra export.

Default target is the original EXE export of DEFAULT_BUILD (research/ghidra_full/<folder>); `--build 1fbe6be7|1a4a447d` selects another build.
`--ref <name-part|all>` switches to reference-mod DLL exports (research/ghidra_refs/<name>_<sha16>/).
`gamerefs <original address>` lists reference-mod functions/data that hold that original address as a constant.
`uislot <vtable|function rva>`, `uiclass <name regex>`, `uimap <name regex>`: RTTI/UI map of the original builds.
Every export folder may carry index.sqlite (built by tools/ghidra_index.py); a fresh index is used automatically,
otherwise the TSVs/shards are read as before. Set GHIDRA_NO_INDEX=1 to force the TSV path.
"""
import argparse, bisect, csv, json, os, re, sqlite3, sys
csv.field_size_limit(sys.maxsize)
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]/'research'
BUILDS={'1fbe6be7':('1fbe6be7466a6646','1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6'),
        '1a4a447d':('1a4a447d14e68c40','1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa')}
DEFAULT_BUILD='1a4a447d'
def build_dir(build=None):return BASE/'ghidra_full'/BUILDS[build or DEFAULT_BUILD][0]
ORIG=build_dir()
REFS=BASE/'ghidra_refs'
ORIG_BASE=0x140000000
def addr(s,base=ORIG_BASE):
    n=int(s,16)
    return n-base if n>=base else n
class Root:
    def __init__(self,path,label=None):
        self.path=path;self.label=label or path.name;self._f=None;self._s=None;self._ix=False
        try:
            self.base=int(json.loads((path/'manifest.json').read_text(encoding='utf-8')).get('image_base','140000000'),16)
        except (OSError,ValueError): self.base=ORIG_BASE
    def index(self):
        """Fresh SQLite index (tools/ghidra_index.py) or None."""
        if self._ix is False:
            self._ix=None
            if not os.environ.get('GHIDRA_NO_INDEX'):
                try:
                    here=os.path.dirname(os.path.abspath(__file__))
                    if here not in sys.path: sys.path.insert(0,here)
                    import ghidra_index
                    self._ix=ghidra_index.open_index(self.path)
                except (ImportError,sqlite3.Error,OSError,ValueError,KeyError): self._ix=None
        return self._ix
    def rows(self,name):
        with (self.path/name).open(encoding='utf-8',newline='') as f:
            yield from csv.DictReader(f,delimiter='\t',quoting=csv.QUOTE_NONE)
    def functions(self):
        if self._f is None:
            self._f=list(self.rows('functions.tsv'));self._s=[int(f['rva'],16) for f in self._f]
        return self._f
    def find_func(self,address):
        ix=self.index()
        if ix is not None:
            f=ix.find_func(address)
            if f is not None: f['ix']=True
            return f
        n=addr(address,self.base);fs=self.functions()
        i=bisect.bisect_right(self._s,n)-1
        if i>=0 and int(fs[i]['rva'],16)<=n<=int(fs[i]['end'],16): return fs[i]
        return next((f for f in fs if int(f['rva'],16)<=n<=int(f['end'],16)),None)
    def body(self,f):
        if f.get('ix'):
            yield from self.index().body(f);return
        with (self.path/f['shard']).open(encoding='utf-8') as file:
            for _ in range(int(f['line'])-1): next(file,None)
            for line in file:
                if line.startswith('// ==== FUNC ') and not line.startswith('// ==== FUNC '+f['rva']+' '): break
                yield line.rstrip('\n')
def select_roots(ref,build=None):
    if ref is None: return [Root(build_dir(build),'orig')]
    if not REFS.is_dir(): raise ValueError('no reference exports: '+str(REFS))
    out=[]
    for d in sorted(REFS.iterdir()):
        mf=d/'manifest.json'
        if not d.is_dir() or not mf.exists(): continue
        try: m=json.loads(mf.read_text(encoding='utf-8'))
        except ValueError: continue
        if m.get('status')!='complete': continue
        if ref.lower()=='all' or ref.lower() in (d.name+' '+str(m.get('source',''))).lower(): out.append(Root(d))
    if not out: raise ValueError('no complete reference export matches: '+ref)
    return out
def query(root,command,value=None,scope='c'):
    ix=root.index() if command!='status' else None
    if ix is not None:
        gen=ix.query(command,value,scope)
        try: first=next(gen)
        except StopIteration: return
        except sqlite3.Error: ix=None
        else:
            yield first;yield from gen;return
    yield from query_scan(root,command,value,scope)
def query_scan(root,command,value=None,scope='c'):
    names=lambda: {f['rva']:f['name'] for f in root.functions()}
    if command=='status':
        m=json.loads((root.path/'manifest.json').read_text(encoding='utf-8'))
        yield json.dumps({'status':m['status'],'sha256':m['sha256'],'source':m.get('source'),'completed_shards':len(m.get('completed_shards',[])) or None,'total_shards':m.get('counts',{}).get('total_shards'),'counts':m.get('counts',{})},ensure_ascii=False);return
    if command=='func':
        f=root.find_func(value)
        if f is None: raise ValueError('function not found in completed shards')
        yield from root.body(f);return
    if command=='grep':
        pat=re.compile(value,re.I)
        if scope=='c':
            for f in root.functions():
                for line in root.body(f):
                    if pat.search(line): yield f["rva"]+'\t'+f['name']+'\t'+line
        else:
            nm=names()
            for row in root.rows(scope+'.tsv'):
                if pat.search('\t'.join(row.values())):
                    refs=row.get('ref_functions','').split(',') if scope=='strings' else [row['rva']]
                    for ref in refs: yield ref+'\t'+nm.get(ref,'')+'\t'+'\t'.join(row.values())
        return
    if command in ('str','imports','exports'):
        pat=re.compile(value or '.',re.I)
        nm=names() if command=='str' else {}
        fname={'str':'strings.tsv','imports':'imports.tsv','exports':'exports.tsv'}[command]
        if not (root.path/fname).exists(): raise ValueError(fname+' not available for this export')
        for row in root.rows(fname):
            if pat.search('\t'.join(row.values())):
                suffix='\t'+','.join(r+':'+nm.get(r,'') for r in row['ref_functions'].split(',') if r) if command=='str' else ''
                yield '\t'.join(row.values())+suffix
        return
    n=addr(value,root.base); target='%08X'%n
    if command in ('callers','callees'):
        f=root.find_func(value)
        if f: target=f['rva']
        key='callee_rva' if command=='callers' else 'caller_func_rva'
        for row in root.rows('calls.tsv'):
            if row[key]==target: yield '\t'.join(row.values())
    elif command=='xref':
        for row in root.rows('datarefs.tsv'):
            if row['to_rva']==target: yield '\t'.join(row.values())
    elif command=='vtable':
        for row in root.rows('vtables.tsv'):
            if target in (row['vftable_rva'],row['target_rva']): yield '\t'.join(row.values())
def gamerefs(value,build=None):
    """Reference-mod code/data holding the original address (or its containing function start) as a constant."""
    # Reference-mod constants were extracted against the old base build 1fbe6be7 (game_rvas.tsv); default to it.
    build=build or '1fbe6be7'
    n=addr(value);target='%08X'%n
    oroot=Root(build_dir(build),'orig');oix=oroot.index()
    of=oix.find_func(value) if oix is not None else oroot.find_func(value)
    starts={target}
    if of: starts.add(of['rva'])
    yield '# original: %s (inside function %s %s)'%(target,of['rva'] if of else '?',of['name'] if of else '')
    lit=re.compile(r'0x(?:14)?0*%x\b'%n,re.I)
    for root in select_roots('all'):
        # each ref export records the game build its constants target (manifest game_build; older exports = 1fbe6be7)
        try: gb=json.loads((root.path/'manifest.json').read_text(encoding='utf-8')).get('game_build','1fbe6be7')
        except (OSError,ValueError): gb='1fbe6be7'
        if gb!=build: continue
        ix=root.index()
        if ix is not None:
            yield from ix.game_rvas(root.label,{int(x,16) for x in starts},n)
            yield from ix.c_literals(root.label,n)
            continue
        nm={f['rva']:f['name'] for f in root.functions()}
        p=root.path/'game_rvas.tsv'
        if p.exists():
            for row in root.rows('game_rvas.tsv'):
                if row['game_rva'] in starts:
                    fr=row['func_rva']
                    yield '['+root.label+']	'+row['form']+'	loc='+row['loc_rva']+'	func='+(fr+' '+nm.get(fr,'') if fr else '-')+'	'+row['value']+('	(function start)' if row['game_rva']!=target else '')
        for f in root.functions():
            for line in root.body(f):
                if lit.search(line):
                    yield '['+root.label+']	c-literal	func='+f['rva']+' '+f['name']+'	'+line.strip()[:160]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--limit',type=int,default=200);ap.add_argument('--ref',default=None,help='reference-mod export: name part or "all"');ap.add_argument('--build',choices=sorted(BUILDS),default=None,help='original EXE build (default %s)'%DEFAULT_BUILD)
    sub=ap.add_subparsers(dest='command',required=True)
    for name in ('func','grep','callers','callees','xref','str','vtable','imports','exports','gamerefs','uislot','uiclass','uimap'):
        p=sub.add_parser(name);p.add_argument('value',nargs='?' if name in ('exports','str','imports') else None);p.add_argument('--limit',type=int,default=argparse.SUPPRESS);p.add_argument('--ref',default=argparse.SUPPRESS);p.add_argument('--build',choices=sorted(BUILDS),default=argparse.SUPPRESS)
        if name=='grep':p.add_argument('--in',dest='scope',choices=('c','strings','symbols'),default='c')
    p=sub.add_parser('status');p.add_argument('--limit',type=int,default=argparse.SUPPRESS);p.add_argument('--ref',default=argparse.SUPPRESS);p.add_argument('--build',choices=sorted(BUILDS),default=argparse.SUPPRESS)
    a=ap.parse_args()
    if a.limit<1:ap.error('--limit must be positive')
    try:
        if a.command=='gamerefs':
            lines=gamerefs(a.value,a.build)
        elif a.command in ('uislot','uiclass','uimap'):
            if a.ref is not None: raise ValueError(a.command+' exists only for the original EXE exports')
            ix=Root(build_dir(a.build),'orig').index()
            if ix is None: raise ValueError('no fresh index: run python -X utf8 tools/ghidra_index.py')
            lines=getattr(ix,a.command)(a.value)
        else:
            roots=select_roots(a.ref,a.build)
            if a.command=='exports' and a.ref is None: raise ValueError('exports exists only for --ref exports')
            def gen():
                for root in roots:
                    try:
                        for line in query(root,a.command,getattr(a,'value',None),getattr(a,'scope','c')):
                            yield ('['+root.label+']\t'+line) if len(roots)>1 else line
                    except ValueError as e:
                        if len(roots)==1: raise
            lines=gen()
        for index,line in enumerate(lines):
            if index>=a.limit:break
            print(line)
    except (OSError,ValueError) as e: print(str(e),file=sys.stderr);return 1
    return 0
if __name__=='__main__':sys.exit(main())
