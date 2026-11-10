"""U35 2026-11-10: loading/busy bins must not masquerade as date stalls."""
import importlib.util
from pathlib import Path
import tempfile
from datetime import datetime
import unittest

spec = importlib.util.spec_from_file_location('cpu_scope_report', Path(__file__).resolve().parents[1] / 'tools/cpu_scope_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class ScopeReportTests(unittest.TestCase):
    def test_window_process_and_stack_thread(self):
        text = '''OS Version: test, Events Lost: 0, Buffers lost: 0, Trace Start: 134387591450000000,
I-Start,0,SAN14CE_KO.exe (123),0x100000,0x200000,\\san14ce_ko.exe
SampledProfile,500000,SAN14CE_KO.exe (123),7,a,b,c,d,400
Stack,500000,7,1,0x186E920
SampledProfile,1500000,SAN14CE_KO.exe (123),7,a,b,c,d,100
Stack,1500000,8,1,0x186E920
Stack,1500000,7,1,0x186E920
SampledProfile,1600000,SAN14CE_KO.exe (999),7,a,b,c,d,900
Stack,1600000,7,1,0x186E920
SampledProfile,2500000,SAN14CE_KO.exe (123),7,a,b,c,d,400
Stack,2500000,7,1,0x186E920
'''.replace('0x200000', '0x2000000')
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'mock.csv'
            path.write_text(text, encoding='utf-8')
            data = report.summarize(path, 123, {'7'},
                datetime.fromisoformat('2026-11-10T13:39:06+09:00'),
                datetime.fromisoformat('2026-11-10T13:39:07+09:00'),
                [(0x176E920, 0x176E930, '86E920')])
        row = data['threads']['7']
        self.assertEqual(row['all']['samples_ms'], 100)
        self.assertEqual(row['all']['inc EXE:86E920'], 100)
        self.assertEqual(row['all']['leaf EXE:86E920'], 100)
        self.assertEqual(row['busy_bins'], 0)
        self.assertEqual(data['lost_events_buffers'], [0, 0])


if __name__ == '__main__':
    unittest.main()
