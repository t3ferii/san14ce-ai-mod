import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Check generated trace inventories without writing source or research files."""
from pathlib import Path
import runpy

project = Path(__file__).resolve().parents[1]
expected = {project / 'research/ai_trace/bt_nodes.json', project / 'src/ai_trace_nodes.h'}
seen = set()
original = Path.write_text

def compare(path, data, *args, **kwargs):
    if path not in expected:
        raise AssertionError(f'unexpected generator output: {path}')
    seen.add(path)
    # read_text is GA-expanded by ga_expand; expand the generated text the same way (user 2026-11-05 patch resilience)
    if path.read_text(encoding='utf-8') != (ga_expand.expand(data) if path.suffix in ga_expand._EXT else data):
        raise AssertionError(f'generated trace inventory differs: {path}')
    return len(data)

try:
    Path.write_text = compare
    runpy.run_path(str(project / 'tools/gen_ai_trace_nodes.py'), run_name='__main__')
finally:
    Path.write_text = original
if seen != expected:
    raise AssertionError(f'missing generator outputs: {expected - seen}')
print('{"ai_trace_generated_outputs_checked": 2}')
