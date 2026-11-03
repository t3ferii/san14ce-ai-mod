"""Shared logger records may interleave unrelated events; report only the latest game session."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('terrain_analysis', Path(__file__).resolve().parents[1]/'tools/analyze_terrain_profile.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)

class ParseTests(unittest.TestCase):
    def test_latest_session_and_paired_peak(self):
        def line(name, a, b, c):
            return f'2026-11-02 01:00:00 day=4 thread=1 {name} army={a} value={b} hex={c}'
        reports = analysis.parse_lines([
            line('terrain_profile_on', 8, 75000, 1000000), line('terrain_frame', 900, 999000, 1000000),
            line('terrain_profile_on', 16, 75000, 1000000), line('terrain_frame', 35, 500000, 900000),
            line('perf_spike', 500, 1, 0), line('terrain_wall_readback_Map', 35, 480000, 470000),
            line('terrain_cpu_readback_Map', 35, 1000, 800), line('terrain_peak_readback_Map', 34, 470000, 600),
            line('terrain_wall_wrong_window', 99, 1, 2), line('terrain_frame_cpu', 35, 4000, 0xFFFFFFFF),
        ])
        self.assertEqual(len(reports), 1)
        self.assertEqual(reports[0]['processors'], 16)
        self.assertEqual(reports[0]['functions']['readback_Map']['peak'], [34, 470000, 600])
        self.assertNotIn('wrong_window', reports[0]['functions'])
        self.assertIsNone(reports[0]['present_thread_cpu_us'])

    def test_empty(self):
        self.assertEqual(analysis.parse_lines(['unrelated line']), [])

if __name__ == '__main__':
    unittest.main()
