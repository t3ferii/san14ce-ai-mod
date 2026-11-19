"""Register a new 기능설정.ini switch in the two JSON registries, then regenerate.

Does the mechanical, error-prone part of FEATURE_CHANGE_CHECKLIST §2-3 in one go:
  1. development/installer_map.json: insert the item (after its parent row and the
     parent's existing children, else after the last row of the same tab+section)
  2. development/module_registry.json: append the flag to "flags" (bool member of ModFeatures)
  3. --apply: gen_module_manifests / gen_installer_catalog / gen_module_table --apply,
     then gen_installer_catalog --check and feature_deps --check
It prints the two C++ lines to place by hand (src/native_core.inl struct field and
load_mod_features line) — their position and the parent guard need judgment.

  python -B -X utf8 tools/add_setting.py --section combat --key foo --module ai \
      --tab 전투 --group combat --default 0 --label "이름" --note "원작: …\n변경: …\n효과: …" \
      [--requires combat.mission_v2] [--code-default false] [--apply]
  python -B -X utf8 tools/add_setting.py --selftest

Without --apply it only shows the plan (dry run).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAP = ROOT / "development" / "installer_map.json"
REG = ROOT / "development" / "module_registry.json"
FEATURES = ROOT / "development" / "features.json"
GENERATORS = [
    ["tools/gen_module_manifests.py", "--apply"],
    ["tools/gen_installer_catalog.py", "--apply"],
    ["tools/gen_module_table.py", "--apply"],
    ["tools/gen_installer_catalog.py", "--check"],
    ["tools/feature_deps.py", "--check"],
]
KEY_RX = re.compile(r"^[a-z][a-z0-9_]*$")


def plan_item(items: list, modules: dict, a) -> tuple[dict, int, list[str]]:
    errs: list[str] = []
    if not KEY_RX.match(a.key) or not KEY_RX.match(a.section):
        errs.append("section/key must be lower_snake_case")
    if any(i["section"] == a.section and i["key"] == a.key for i in items):
        errs.append(f"[{a.section}] {a.key} already exists")
    if a.module not in modules:
        errs.append(f"module '{a.module}' not in features.json")
    tabs = {i["tab"] for i in items}
    if a.tab not in tabs:
        errs.append(f"tab '{a.tab}' unknown; one of: {', '.join(sorted(tabs))}")
    parent_idx = None
    if a.requires:
        ps, _, pk = a.requires.partition(".")
        parent_idx = next((n for n, i in enumerate(items) if i["section"] == ps and i["key"] == pk), None)
        if parent_idx is None:
            errs.append(f"requires '{a.requires}' is not an installer item")
        elif items[parent_idx]["tab"] != a.tab:
            errs.append(f"parent is on tab '{items[parent_idx]['tab']}'; a └ child must share the parent's tab (checklist §2)")
    item = {"tab": a.tab, "group": a.group, "section": a.section, "key": a.key,
            "default": a.default, "requires": a.requires or "", "inherit": "",
            "label": a.label, "note": a.note.replace("\\n", "\n"), "retired": False, "module": a.module}
    if parent_idx is not None:
        # children are not contiguous in the map: go after the parent's last descendant
        family = {a.requires}
        last = parent_idx
        for n in range(parent_idx + 1, len(items)):
            if items[n].get("requires") in family:
                family.add(f"{items[n]['section']}.{items[n]['key']}")
                last = n
        pos = last + 1
    else:
        same = [n for n, i in enumerate(items) if i["tab"] == a.tab and i["section"] == a.section]
        tab = [n for n, i in enumerate(items) if i["tab"] == a.tab]
        pos = (same or tab or [len(items) - 1])[-1] + 1
    return item, pos, errs


def add_flag(reg_text: str, flag: str) -> str:
    m = re.search(r'("flags":\s*\[)([^\]]*)\]', reg_text)
    if not m:
        raise SystemExit("module_registry.json: flags list not found")
    flags = json.loads("[" + m.group(2) + "]")
    if flag in flags:
        return reg_text
    return reg_text[: m.end(2)] + f', "{flag}"' + reg_text[m.end(2):]


def cpp_lines(a) -> list[str]:
    cd = a.code_default
    parent = a.requires.partition(".")[2] if a.requires else ""
    guard = f"f.{parent} && " if parent else ""
    return [
        f"    bool {a.key} = false;   // [{a.section}] <local번호>: <한 줄 설명>",
        f"    f.{a.key} = {guard}feature_key(config, L\"{a.section}\", L\"{a.key}\", {cd});",
    ]


def selftest() -> int:
    items = [
        {"tab": "전투", "section": "combat", "key": "mission_v2", "requires": ""},
        {"tab": "전투", "section": "combat", "key": "kid", "requires": "combat.mission_v2"},
        {"tab": "전투", "section": "combat", "key": "other", "requires": ""},
        {"tab": "전투", "section": "combat", "key": "grandkid", "requires": "combat.kid"},
        {"tab": "편의성", "section": "x", "key": "y", "requires": ""},
    ]
    mods = {"ai": {}}
    ns = argparse.Namespace(section="combat", key="new_one", module="ai", tab="전투", group="combat",
                            default=0, requires="combat.mission_v2", label="L", note="원작: a\\n변경: b", code_default="false")
    item, pos, errs = plan_item(items, mods, ns)
    assert not errs and pos == 4 and item["note"] == "원작: a\n변경: b", (pos, errs)
    ns.requires = ""; ns.tab = "편의성"; ns.section = "x"
    _, pos, errs = plan_item(items, mods, ns)
    assert not errs and pos == 5, (pos, errs)
    ns.key = "y"
    assert any("already exists" in e for e in plan_item(items, mods, ns)[2])
    ns.key = "z"; ns.requires = "combat.mission_v2"
    assert any("share the parent's tab" in e for e in plan_item(items, mods, ns)[2])
    t = add_flag('{"flags": ["a", "b"],\n "x": 1}', "c")
    assert '"flags": ["a", "b", "c"]' in t, t
    assert "f.mission_v2 && feature_key" in cpp_lines(argparse.Namespace(key="k", section="combat", requires="combat.mission_v2", code_default="false"))[1]
    print("add_setting selftest ok")
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--section"); ap.add_argument("--key"); ap.add_argument("--module")
    ap.add_argument("--tab"); ap.add_argument("--group", default="")
    ap.add_argument("--default", type=int, default=0, help="installer default (user-facing)")
    ap.add_argument("--code-default", default="false", help="feature_key fallback in the DLL; keep the safe value")
    ap.add_argument("--requires", default="", help="parent as section.key")
    ap.add_argument("--label", default=""); ap.add_argument("--note", default="")
    ap.add_argument("--apply", action="store_true"); ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    for need in ("section", "key", "module", "tab", "label", "note"):
        if not getattr(a, need):
            ap.error(f"--{need.replace('_', '-')} required")
    data = json.loads(MAP.read_text(encoding="utf-8"))
    modules = json.loads(FEATURES.read_text(encoding="utf-8"))["modules"]
    item, pos, errs = plan_item(data["items"], modules, a)
    if errs:
        print("refused:\n- " + "\n- ".join(errs))
        return 1
    after = data["items"][pos - 1]
    print(f"installer_map: insert [{a.section}] {a.key} at #{pos} (after [{after['section']}] {after['key']}), module={a.module}, default={a.default}")
    print(f"module_registry: add flag {a.key}")
    print("C++ (place by hand, src/native_core.inl — struct ModFeatures, then load_mod_features):")
    print("\n".join(cpp_lines(a)))
    if not a.apply:
        print("dry run — add --apply to write and regenerate.")
        return 0
    data["items"].insert(pos, item)
    MAP.write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    reg_raw = REG.read_bytes().decode("utf-8")
    REG.write_bytes(add_flag(reg_raw, a.key).encode("utf-8"))
    for cmd in GENERATORS:
        r = subprocess.run([sys.executable, "-B", "-X", "utf8", *cmd], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        tail = (r.stdout + r.stderr).strip().splitlines()[-3:]
        print(f"{'ok ' if r.returncode == 0 else 'FAIL'} {' '.join(cmd)}" + ("" if r.returncode == 0 else "\n  " + "\n  ".join(tail)))
        if r.returncode != 0 and cmd[-1] == "--apply":
            return r.returncode
    print("next: place the C++ lines, add the switch to docs/FEATURE_SWITCHES.md, new requires pair → tests/verify_installer_gui.py expected_requires, "
          "then check_feature.ps1 -Feature installer-gui and the module's route; install with -Gui.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
