"""Per-release feature catalogue for the installer's changed-feature colours (user 2026-11-01).

  python -B -X utf8 tools/release_features.py <current version> <output json>

Reads the FeatureSelect.ps1 catalogue inside each published dist ZIP (v13.3 and later) plus the
working catalogue as <current version>. Each feature id maps to a short digest of its label, default,
parent and note, so the installer can tell new / changed / removed features between any two versions.
"""
from pathlib import Path
import hashlib,json,re,sys,zipfile

P=Path(__file__).resolve().parents[1]
ROW=re.compile(r"@\{Group='[\w]*'; Section='(\w+)'; Key='(\w+)'; Default=(\d); Requires='([\w.]*)'; Inherit='[\w.]*'; Tab='[^']+'; (?:Retired=\$true; )?"
               r"Label='((?:[^']|'')*)'; Note='((?:[^']|'')*)'")
PUBLISHED=('v13.3','v14','v14.1','v14.2','v15','v15.1','v15.2')

def catalogue(text):
    rows={}
    for section,key,default,requires,label,note in ROW.findall(text):
        digest=hashlib.sha1('\x1f'.join((label,default,requires,note)).encode('utf-8')).hexdigest()[:12]
        rows[section+'.'+key]=digest
    assert rows,'no catalogue rows'
    return rows

def main():
    current,output=sys.argv[1],Path(sys.argv[2])
    versions={}
    for v in PUBLISHED:
        with zipfile.ZipFile(P/'dist'/('SAN14_AI_'+v+'_integrated.zip')) as z:
            versions[v]=catalogue(z.read('AI_TacticsMod/FeatureSelect.ps1').decode('utf-8-sig'))
    menu=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
    visible="\n".join(line for line in menu.splitlines() if "Retired=$true;" not in line)
    versions[current]=catalogue(visible)
    assert len(versions[current])==menu.count('@{Group=')-menu.count('Retired=$true;'),'unparsed catalogue rows'
    output.write_text(json.dumps({'format':1,'versions':versions},ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    print(json.dumps({v:len(r) for v,r in versions.items()}))

if __name__=='__main__':main()
