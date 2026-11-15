"""Read existing SAN14_AITactics.log; compare frame latency with terrain call and CPU cost."""
from pathlib import Path
import argparse
import json
import re

EVENT = re.compile(r'\b(terrain_\w+) army=(\d+) value=(\d+) hex=(\d+)')
METRIC = re.compile(r'terrain_(peak_thread|wall|cpu|self|calls|cycles|range|peak|valid)_(.+)')

def parse_lines(lines):
    reports = []
    current = None
    processors = None
    for line in lines:
        match = EVENT.search(line)
        if not match:
            continue
        kind, a, b, c = match.groups()
        a, b, c = int(a), int(b), int(c)
        if kind == 'terrain_profile_on':
            reports, current, processors = [], None, a
        elif kind == 'terrain_frame':
            current = dict(frame=a, frame_us=b, window_us=c, processors=processors, functions={})
            reports.append(current)
        elif current is not None:
            if kind == 'terrain_frame_cpu' and a == current['frame']:
                current.update(process_frame_cpu_us=b, present_thread_cpu_us=None if c == 0xFFFFFFFF else c)
            elif kind == 'terrain_cpu_window' and a == current['frame']:
                current.update(process_window_cpu_us=b, window_us=c)
            else:
                metric = METRIC.fullmatch(kind)
                if metric:
                    metric, name = metric.groups()
                    if metric.startswith('peak') or a == current['frame']:
                        current['functions'].setdefault(name, {})[metric] = [a, b, c]
    return reports

def render_report(report, limit):
    cpu = report.get('process_frame_cpu_us')
    print(f"frame={report['frame']}  Present 간격={report['frame_us']/1000:.3f}ms  "
          f"프로세스 CPU={cpu/1000:.3f}ms" if cpu is not None else f"frame={report['frame']}  Present 간격={report['frame_us']/1000:.3f}ms")
    print(f"집계 창={report['window_us']/1000:.3f}ms; inclusive 합산 금지. self는 계측된 자식 호출을 제외합니다.")
    print('함수 | 호출 | inclusive ms | self ms | CPU ms | 최장 호출 wall/CPU ms | peak frame')
    rows = sorted(report['functions'].items(), key=lambda item: item[1].get('self', [0, 0, 0])[1], reverse=True)
    for name, values in rows[:limit]:
        count = values.get('calls', [0, 0, 0])[1]
        wall = values.get('wall', [0, 0, 0])[1]
        own = values.get('self', [0, 0, 0])[1]
        cpu = values.get('cpu', [0, 0, 0])[1]
        peak_frame, peak_wall, peak_cpu = values.get('peak', [0, 0, 0])
        valid = values.get('valid', [0, 0, 0])[1]
        cpu_text = f'{cpu/1000:.3f}' if valid == count else f'{cpu/1000:.3f} ({valid}/{count} valid)'
        print(f'{name} | {count} | {wall/1000:.3f} | {own/1000:.3f} | {cpu_text} | '
              f'{peak_wall/1000:.3f}/{peak_cpu/1000:.3f} | {peak_frame}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', nargs='?', type=Path, default=Path(__file__).resolve().parents[2]/'SAN14_AITactics.log')
    parser.add_argument('--frames', type=int, default=3)
    parser.add_argument('--rows', type=int, default=12)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    with args.log.open(encoding='utf-8-sig', errors='replace') as log:
        reports = parse_lines(log)
    if not reports:
        parser.error('terrain_profile_on/terrain_frame 기록이 없습니다. 지형 CPU 계측판·진단 로그 및 terrain_edit 설정을 확인하세요.')
    selected = sorted(reports, key=lambda item: item['frame_us'], reverse=True)[:max(1, args.frames)]
    if args.json:
        print(json.dumps(selected, ensure_ascii=False, indent=2))
    else:
        for report in selected:
            render_report(report, max(1, args.rows))

if __name__ == '__main__':
    main()
