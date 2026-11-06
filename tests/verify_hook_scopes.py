import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Audit statically named hook addresses and report missing entry scopes."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
files={p:p.read_text(encoding="utf-8") for p in (root/"src").iterdir() if p.suffix in (".cpp",".inl",".h")}
targets=set()
for path,source in files.items():
    if path.name=="gpu_profile.cpp":
        targets.update((path,name) for name in re.findall(r"patch\([^\n]*?reinterpret_cast<void \*>\((\w+)\)",source))
    for window in re.findall(r"SetWindowLongPtrW\([^;]*reinterpret_cast<LONG_PTR>\(&([\w:]+)\)",source):
        targets.add((path,window))
    for line in source.splitlines():
        if "reinterpret_cast<void *>(&" in line:
            targets.update((path,name) for name in re.findall(r"reinterpret_cast<void \*>\(&([\w:]+)(?:<\d+>)?\)",line))
# Typed wrapper arrays are relayed without an explicit address-of operator.
for path,source in files.items():
    for match in re.finditer(r"\b(?:Face|ProgressPhase)\s+wrappers\s*\[[^]]*\]\s*=\s*\{([^}]*)\}",source):
        for name in re.findall(r"\b[A-Za-z_]\w*\b",match.group(1)):
            targets.add((path,name))
allow={"domestic_officer_node":"MSVC C2712: SEH finally forbids RAII scope; entry crumb retained",
       "terrain_edit::hex_menu_state":"MSVC C2712: SEH finally forbids RAII scope; entry crumb retained",
       "progress_native_ai":"MSVC C2712: SEH finally forbids RAII scope; entry crumb retained"}
missing=[];found=0;allowed=set()
for path,name in sorted(targets,key=lambda x:(str(x[0]),x[1])):
    short=name.split("::")[-1]
    if name in allow or short in allow:
        key=name if name in allow else short
        check=re.compile(r"\b"+re.escape(short)+r"\s*\([^;{}]*\)\s*\{\s*COST_CRUMB\s*\(",re.S)
        if not any(check.search(text) for text in files.values()):missing.append(f"{name}: allowed hook lacks entry crumb")
        else:allowed.add(key)
        continue
    candidates=[]
    # Qualified split table references live in another file; search every source.
    pool=files.items() if "::" in name or path.name=="native_split_install.inl" else [(path,files[path])]
    pattern=re.compile(r"\b"+re.escape(short)+r"\s*\([^;{}]*\)\s*(?:noexcept\s*)?\{\s*(?:/\*.*?\*/\s*)?(COST_SCOPE\s*\() ?",re.S)
    bare=re.compile(r"\b"+re.escape(short)+r"\s*\([^;{}]*\)\s*(?:noexcept\s*)?\{",re.S)
    for q,text in pool:
        if bare.search(text):candidates.append((q,bool(pattern.search(text))))
    if not candidates:
        for q,text in files.items():
            if bare.search(text):candidates.append((q,bool(pattern.search(text))))
    if not candidates:missing.append(f"{path.name}: {name}: definition unresolved")
    elif not any(ok for _,ok in candidates):missing.append(f"{path.name}: {name}: missing COST_SCOPE")
    else:found+=1
print(f"hook references={len(targets)} scoped={found} allowed={len(allowed)} missing={len(missing)}")
for name in sorted(allowed):print(f"allow {name}: {allow[name]}")
for item in missing:print(item)
raise SystemExit(bool(missing))
