"""Check that CPU stacks are joined by timestamp AND thread, not adjacency."""
from pathlib import Path
import csv,json,subprocess,sys,tempfile
p=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='sample_fixture_',dir=p/'build') as temp:
    folder=Path(temp);data=folder/'samples.csv';output=folder/'summary.json'
    rows=[
        ['SampledProfile','TimeStamp','Process','Thread','IP','CPU','Start','Function','Count','Type'],
        ['SampledProfile','1000','SAN14CE_KO.exe (12404)','7','0x100020','0','unknown','SAN14CE_KO.exe!0x100020','2','Unbatched'],
        ['Stack','1000','7','1','0x100020','SAN14CE_KO.exe!0x100020'],
        ['Stack','1000','7','2','0x100020','SAN14CE_KO.exe!0x100020'],
        ['Stack','1001','7','3','0x100099','SAN14CE_KO.exe!0x100099'],
        ['Stack','1000','8','4','0x100077','SAN14CE_KO.exe!0x100077'],
        ['SampledProfile','2000','other.exe (5)','7','0x100099','0','unknown','other.exe!0x100099','1','Unbatched'],
        ['Stack','2000','7','1','0x100088','other.exe!0x100088'],
        ['SampledProfile','5000100','SAN14CE_KO.exe (12404)','9','0x100030','0','unknown','SAN14CE_KO.exe!0x100030','1','Unbatched'],
    ]
    with data.open('w',encoding='utf-8',newline='') as f:csv.writer(f).writerows(rows)
    subprocess.run([sys.executable,str(p/'tools/analyze_progress_samples.py'),str(data),'--pid','12404',
        '--base','0x100000','--out',str(output),'--lost-events','12'],check=True,capture_output=True)
    report=json.loads(output.read_text(encoding='utf-8'))
    assert report['retained_cpu_sample_count']==3
    assert report['sample_records_with_matched_stack']==1
    assert report['top_game_stack_rvas']==[{'name':'SAN14CE_KO.exe+0x20','retained_count':2}]
    assert len(report['five_second_threads'])==2
    assert not report['quantitative_percentages_reliable'] and report['events_lost_in_source_trace']==12
    assert report['top_threads'][0]['name']=='7'
result={'status':'sampled_cpu_parser_fixture_passed','checks':6,'live_game_modified':False}
(p/'build/progress_samples_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
