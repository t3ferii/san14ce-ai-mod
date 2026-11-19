"""Compact summary of the in-game mod logs (read-only, no game access).

The SAN14_*.log files are 10-25 MB; reading them in a chat costs far more than
this summary. Default: the last launch in SAN14_AITactics.log and
SAN14_CombatAI.log — wall time, game-date range, exit kind, exceptions, loads,
perf spikes, top events and events that are new or gone versus the previous
launch. Then narrow with --event / --grep.

  python -B -X utf8 tools/log_summary.py                      # last launch, both logs
  python -B -X utf8 tools/log_summary.py --event helm_turn --show 8
  python -B -X utf8 tools/log_summary.py --grep "army=54 " --log combat
  python -B -X utf8 tools/log_summary.py --session -2         # the launch before
  python -B -X utf8 tools/log_summary.py --selftest

Line format: "YYYY-MM-DD HH:MM:SS day=N thread=T event army=A value=V hex=H".
day = year*360 + (month-1)*30 + (day-1); -1 before a game is loaded. The day
column goes stale once the AI stops. The logs never print 멸망/통일 text.
"""
from __future__ import annotations

import argparse
import collections
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME_CANDIDATES = (ROOT.parent, ROOT.parent.parent)
LOGS = {"tactics": "SAN14_AITactics.log", "combat": "SAN14_CombatAI.log"}
LINE = re.compile(r"^(\S+ \S+) day=(-?\d+) thread=(\d+) (\S+)(?: army=(-?\d+))?(?: value=(-?\d+))?(?: hex=(\S+))?")
START_EVENTS = {"exception_diagnostic_ready"}


def game_dir() -> Path:
    for p in GAME_CANDIDATES:
        if (p / LOGS["tactics"]).exists() or (p / LOGS["combat"]).exists():
            return p
    return ROOT.parent


def day_text(d: int) -> str:
    if d < 0:
        return "-"
    y, r = divmod(d, 360)
    m, dd = divmod(r, 30)
    return f"{y}.{m + 1}.{dd + 1}"


def sessions(lines: list[str], cut_times: list[str] | None = None) -> list[tuple[int, int]]:
    """Launch boundaries. The tactics log marks each launch; the combat log does
    not, so it is cut at the tactics log's launch timestamps (cut_times)."""
    if cut_times:
        starts, k = [], 0
        for i, l in enumerate(lines):
            if not (l[:4].isdigit() and l[4:5] == "-"):
                continue  # continuation/garbage lines would compare above any timestamp
            while k < len(cut_times) and l[:19] >= cut_times[k]:
                if not starts or starts[-1] != i:
                    starts.append(i)
                k += 1
    else:
        starts = [i for i, l in enumerate(lines) if any(f" {e} " in l for e in START_EVENTS)]
    if not starts or starts[0] != 0:
        starts = [0] + starts
    return [(s, (starts[k + 1] if k + 1 < len(starts) else len(lines))) for k, s in enumerate(starts)]


def parse(lines: list[str]):
    for l in lines:
        m = LINE.match(l)
        if m:
            yield m


def summarize(name: str, lines: list[str], prev: list[str] | None, top: int) -> list[str]:
    rows = list(parse(lines))
    if not rows:
        return [f"## {name}: no parsable lines"]
    days = [int(r.group(2)) for r in rows if int(r.group(2)) >= 0]
    ev = collections.Counter(r.group(4) for r in rows)
    out = [f"## {name}: {rows[0].group(1)} → {rows[-1].group(1)}, {len(rows)} lines"]
    if days:
        out.append(f"game date: {day_text(min(days))} → {day_text(max(days))} (day {min(days)}→{max(days)}, {max(days) - min(days)} days)")
    else:
        out.append("game date: no game loaded (title/menus only)")
    exc = [r for r in rows if r.group(4).startswith("exception_") and r.group(4) != "exception_diagnostic_ready"]
    last = rows[-1].group(4)
    if exc:
        kind = f"EXCEPTION x{len(exc)} (first: {exc[0].group(0)[:160]})"
    elif last == "foam_destroy" or ev.get("foam_destroy"):
        kind = "orderly exit (foam_destroy)"
    elif "Combat" in name:
        kind = "no exit marker in this log by design — use the AITactics log's exit line"
    else:
        kind = f"no exit marker — still running, killed, or log cut (last event {last})"
    out.append(f"exit: {kind}")
    loads = ev.get("load_host_phase", 0)
    title = sum(1 for r in rows if r.group(4) == "layout_p4_screen" and r.group(5) == "4" and r.group(6) == "39")
    out.append(f"loads(load_host_phase): {loads}  title-screen marks: {title}")
    spikes = [int(r.group(5) or 0) for r in rows if r.group(4) == "perf_spike"]
    if spikes:
        spikes.sort()
        out.append(f"perf_spike: {len(spikes)} (gap ms median {spikes[len(spikes) // 2]}, max {spikes[-1]}, ≥100ms {sum(s >= 100 for s in spikes)})")
    out.append(f"top events: " + ", ".join(f"{e} {c}" for e, c in ev.most_common(top)))
    if prev is not None:
        pev = collections.Counter(m.group(4) for m in parse(prev))
        new = sorted(set(ev) - set(pev), key=lambda e: -ev[e])
        gone = sorted(set(pev) - set(ev), key=lambda e: -pev[e])
        out.append("new vs previous launch: " + (", ".join(f"{e} {ev[e]}" for e in new[:top]) or "—"))
        out.append("gone vs previous launch: " + (", ".join(f"{e} {pev[e]}" for e in gone[:top]) or "—"))
    return out


def detail(lines: list[str], event: str | None, pattern: str | None, show: int) -> list[str]:
    rx = re.compile(pattern) if pattern else None
    hit = [l for l in lines if (event is None or f" {event} " in l + " ") and (rx is None or rx.search(l))]
    if not hit:
        return ["  (no matching lines)"]
    out = [f"  {len(hit)} lines"]
    rows = [m for m in parse(hit)]
    if event and rows:
        armies = collections.Counter(m.group(5) for m in rows)
        vals = [int(m.group(6)) for m in rows if m.group(6)]
        out.append("  army top: " + ", ".join(f"{a}×{c}" for a, c in armies.most_common(8)))
        if vals:
            out.append(f"  value min/max: {min(vals)}/{max(vals)}")
    picks = hit if len(hit) <= show * 2 else hit[:show] + ["  …"] + hit[-show:]
    out += ["  " + p[:200] for p in picks]
    return out


def selftest() -> int:
    fake = [
        "2026-11-19 23:20:51 day=-1 thread=1 exception_diagnostic_ready army=0 value=0 hex=0",
        "2026-11-19 23:20:52 day=-1 thread=1 old_only army=1 value=1 hex=0",
        "2026-11-19 23:21:00 day=-1 thread=1 exception_diagnostic_ready army=0 value=0 hex=0",
        "2026-11-19 23:21:05 day=75150 thread=2 helm_turn army=54 value=3 hex=0",
        "2026-11-19 23:21:06 day=75160 thread=2 perf_spike army=120 value=5 hex=0",
        "2026-11-19 23:21:09 day=75160 thread=2 foam_destroy army=10 value=3 hex=0",
    ]
    ss = sessions(fake)
    assert len(ss) == 2, ss
    a, b = ss[-1]
    text = "\n".join(summarize("t", fake[a:b], fake[ss[0][0]:ss[0][1]], 5))
    assert "208.10.1" in text and "orderly exit" in text and "helm_turn" in text, text
    assert "gone vs previous launch: old_only" in text, text
    assert day_text(75150) == "208.10.1"
    d = "\n".join(detail(fake, "helm_turn", None, 3))
    assert "1 lines" in d and "54×1" in d, d
    print("log_summary selftest ok")
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--log", choices=["tactics", "combat", "both"], default="both")
    ap.add_argument("--session", type=int, default=-1, help="-1 = last launch, -2 = the one before, 0 = whole file")
    ap.add_argument("--old", action="store_true", help="read the .old.log instead")
    ap.add_argument("--event", help="exact event name to detail")
    ap.add_argument("--grep", help="regex filter for detail lines")
    ap.add_argument("--show", type=int, default=5, help="detail lines from each end")
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--dir", type=Path, help="folder holding the logs (default: game folder)")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    base = a.dir or game_dir()
    names = ["tactics", "combat"] if a.log == "both" else [a.log]
    cut_times: list[str] = []
    tpath = base / (LOGS["tactics"].replace(".log", ".old.log") if a.old else LOGS["tactics"])
    if tpath.exists():
        cut_times = [l[:19] for l in tpath.read_text(encoding="utf-8", errors="replace").splitlines()
                     if any(f" {e} " in l for e in START_EVENTS)]
    for n in names:
        fn = LOGS[n].replace(".log", ".old.log") if a.old else LOGS[n]
        path = base / fn
        if not path.exists():
            print(f"## {fn}: missing in {base}")
            continue
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        ss = sessions(lines, cut_times if n == "combat" else None)
        if a.session == 0:
            cur, prev = lines, None
        else:
            if -a.session > len(ss):
                print(f"## {fn}: only {len(ss)} launches")
                continue
            s, e = ss[a.session]
            cur = lines[s:e]
            prev = lines[ss[a.session - 1][0]:ss[a.session - 1][1]] if len(ss) >= -a.session + 1 else None
        print(f"[{fn}] launches in file: {len(ss)}, showing {a.session}")
        if a.event or a.grep:
            print(f"## {fn} detail event={a.event or '*'} grep={a.grep or '-'}")
            print("\n".join(detail(cur, a.event, a.grep, a.show)))
        else:
            print("\n".join(summarize(fn, cur, prev, a.top)))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
