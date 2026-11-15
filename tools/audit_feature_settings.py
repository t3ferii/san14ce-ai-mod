"""Read-only INI/catalog audit; raw profiles and known legacy keys are not checkboxes."""
from pathlib import Path
import argparse, configparser, json, re

P = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ini', type=Path, default=P/'기능설정.ini')
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    gui = (P/'installer/InstallerGui.cs').read_text(encoding='utf-8-sig')
    ps = (P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
    cskeys = {s+'.'+k for s,k in re.findall(r'new Feature\("[^"]+", "[^"]*", "([^"]+)", "([^"]+)"', gui)}
    pskeys = {s+'.'+k for s,k in re.findall(r"Section='([^']+)'; Key='([^']+)'", ps)}
    assert cskeys == pskeys, 'GUI/PS catalogue mismatch'
    raw = set(re.findall(r'"([\w.]+)"', re.search(r'RawSettings = \{(.*?)\}', gui, re.S)[1]))
    data = args.ini.read_bytes()
    ini = configparser.ConfigParser(interpolation=None, strict=True)
    ini.read_string(data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig'))
    legacy = {'game_flow.fast_menu':'performance.fast_menu', 'performance.strategy_batch':'removed: screen corruption'}
    keys = {s+'.'+k for s in ini.sections() for k in ini[s]}
    report = dict(status='feature_settings_audited', catalogue_count=len(cskeys),
                  recognized=sorted(keys & cskeys), raw_settings=sorted(keys & raw),
                  legacy={k:v for k,v in legacy.items() if k in keys},
                  unknown=sorted(keys-cskeys-raw-set(legacy)), duplicate_sections_or_keys=False)
    if args.report:
        args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='recognized'},ensure_ascii=False))

if __name__ == '__main__': main()
