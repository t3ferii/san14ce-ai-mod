"""Compact pre-research pack for one feature (read-only).

Prints, for a module or keyword: module row (dll, deps, dependents, hook owners,
check routes, owned src files), its 기능설정.ini keys with installer defaults and
current values, and matching headings/lines from NATIVE_FUNCTIONS,
MOD_CHANGES_VS_ORIGINAL and USER_REQUESTS. Used before writing a proposal
(san14-orchestrator skill §1) so the research step does not re-read big files.

  python -B -X utf8 tools/feature_brief.py <module-or-keyword> [--kw 단어 ...] [--max N]
  python -B -X utf8 tools/feature_brief.py --selftest
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FEATURES = ROOT / "development" / "features.json"
INSTALLER_MAP = ROOT / "development" / "installer_map.json"
# Worktrees have no 기능설정.ini (untracked); fall back to the installed main folder's copy.
INI = next((p for p in (ROOT / "기능설정.ini", ROOT.parent / "AI_TacticsMod" / "기능설정.ini") if p.exists()), ROOT / "기능설정.ini")
DOCS = {
    "NATIVE_FUNCTIONS": ROOT / "docs" / "NATIVE_FUNCTIONS.md",
    "MOD_CHANGES_VS_ORIGINAL": ROOT / "docs" / "MOD_CHANGES_VS_ORIGINAL.md",
}
REQUESTS = ROOT / "development" / "USER_REQUESTS.md"
LINE_CAP = 220


def read_text(path: Path) -> str:
    raw = path.read_bytes()
    for enc in ("utf-8-sig", "utf-16", "cp949"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", "replace")


def cut(s: str, n: int = LINE_CAP) -> str:
    s = " ".join(s.split())
    return s if len(s) <= n else s[: n - 1] + "…"


def load_ini(path: Path) -> dict[tuple[str, str], str]:
    out: dict[tuple[str, str], str] = {}
    if not path.exists():
        return out
    section = ""
    for line in read_text(path).splitlines():
        t = line.strip()
        if not t or t[0] in ";#":
            continue
        if t.startswith("[") and t.endswith("]"):
            section = t[1:-1].strip()
            continue
        if "=" in t:
            k, v = t.split("=", 1)
            out.setdefault((section, k.strip()), v.split(";", 1)[0].strip())
    return out


def resolve(query: str, modules: dict, items: list) -> tuple[list[str], str]:
    q = query.lower()
    if query in modules:
        m = modules[query]
        if m.get("status") == "alias":
            for d in m.get("depends", []):
                if d.startswith("alias_of="):
                    return [d.split("=", 1)[1]], f"alias {query} →"
        return [query], ""
    hits = [n for n in modules if q in n.lower()]
    for it in items:
        text = f"{it.get('section','')}.{it.get('key','')} {it.get('label','')}".lower()
        if q in text and it.get("module") and it["module"] not in hits:
            hits.append(it["module"])
    for n, m in modules.items():
        if n not in hits and any(q in Path(p).name.lower() for p in m.get("owns", [])):
            hits.append(n)
    hits = [h for h in hits if modules.get(h, {}).get("status") != "alias"]
    return hits, "candidates"


def brief(name: str, modules: dict, items: list, ini: dict, extra_kw: list[str], cap: int) -> list[str]:
    m = modules[name]
    out = [f"# {name}  ({m.get('status')} / {m.get('dll')} / {m.get('build')})"]
    deps = [d for d in m.get("depends", []) if not d.startswith("alias_of=")]
    users = sorted(n for n, o in modules.items() if name in o.get("depends", []))
    out.append(f"depends: {', '.join(deps) or '—'}")
    out.append(f"used by: {', '.join(users) or '—'}")
    if m.get("hook_owners"):
        out.append(f"hook owners: {', '.join(m['hook_owners'])}")
    routes = m.get("tests", [])
    out.append(f"check routes: {', '.join(routes) or '—'}")
    owns = m.get("owns", [])
    src = [p for p in owns if p.startswith("src/")]
    out.append(f"owned src ({len(src)} of {len(owns)} files): " + (", ".join(Path(p).name for p in src[:cap * 2]) or "—"))

    mine = [it for it in items if it.get("module") == name and not it.get("retired")]
    kws = {name, *extra_kw}
    out.append("")
    out.append(f"## settings ({len(mine)}; ini = {INI.relative_to(ROOT.parent) if INI.exists() else '없음'})")
    for it in mine[: cap * 2]:
        sec, key = it.get("section", ""), it.get("key", "")
        kws.add(key)
        cur = ini.get((sec, key))
        req = f" requires={it['requires']}" if it.get("requires") else ""
        out.append(f"- [{sec}] {key} default={it.get('default')} ini={cur if cur is not None else '없음'} tab={it.get('tab')}{req} | {cut(it.get('label',''), 80)}")
        note = (it.get("note") or "").splitlines()
        orig = next((l for l in note if l.startswith("원작")), "")
        if orig:
            out.append(f"    {cut(orig, 160)}")
    if not mine:
        out.append("- (installer_map에 module로 묶인 키 없음)")

    pats = [re.compile(re.escape(k), re.I) for k in sorted(kws) if len(k) >= 3 or not k.isascii()]
    for label, path in DOCS.items():
        if not path.exists():
            continue
        heads = [l for l in read_text(path).splitlines() if l.startswith("#") and any(p.search(l) for p in pats)]
        out.append("")
        out.append(f"## {label} headings ({len(heads)})")
        out += [f"- {cut(h.lstrip('# '))}" for h in heads[-cap:]] or ["- —"]
    if REQUESTS.exists():
        reqs = [l for l in read_text(REQUESTS).splitlines() if l.startswith("- [") and any(p.search(l) for p in pats)]
        out.append("")
        out.append(f"## USER_REQUESTS (latest {min(cap, len(reqs))} of {len(reqs)})")
        out += [f"- {cut(l[2:])}" for l in reqs[-cap:]] or ["- —"]
    out.append("")
    out.append(f"next: tools/check_feature.ps1 -Feature {name} -List ; docs/MODULES.md row `{name}`")
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("query", nargs="?")
    ap.add_argument("--kw", nargs="*", default=[], help="extra words to match in docs (e.g. Korean feature words)")
    ap.add_argument("--max", type=int, default=6, help="lines per doc section")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    modules = json.loads(read_text(FEATURES))["modules"]
    items = json.loads(read_text(INSTALLER_MAP))["items"]
    ini = load_ini(INI)

    if a.selftest:
        assert modules and items, "registry empty"
        name = next(n for n, m in modules.items() if m.get("status") == "active" and any(i.get("module") == n for i in items))
        text = "\n".join(brief(name, modules, items, ini, [], 3))
        assert text.startswith(f"# {name}") and "## settings" in text, text[:300]
        hits, _ = resolve(name[:4], modules, items)
        assert name in hits or hits, "keyword resolve failed"
        print(f"feature_brief selftest ok ({name}, {len(modules)} modules, {len(items)} keys)")
        return 0
    if not a.query:
        ap.error("query required")
    hits, how = resolve(a.query, modules, items)
    if not hits:
        print(f"no module matches '{a.query}'. Try a section/key name or --kw with docs words.")
        return 1
    if len(hits) > 1:
        print(f"{len(hits)} {how} for '{a.query}':")
        for h in hits[:20]:
            m = modules[h]
            n = sum(1 for i in items if i.get("module") == h)
            print(f"- {h} ({m.get('dll')}, {n} keys)")
        print("re-run with one module name.")
        return 0
    if how.startswith("alias"):
        print(how, hits[0])
    print("\n".join(brief(hits[0], modules, items, ini, a.kw, a.max)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
