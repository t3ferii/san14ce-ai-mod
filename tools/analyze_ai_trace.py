"""Analyse an AI trace CSV (native_ai_trace.inl) offline. Read-only; no game access.

Army slots are reused objects: a 'spawn'/'reuse' line with a non-zero commander is a sortie, a
'reuse' to commander 0 is the army leaving the map (arrived, retreated home, destroyed).
For every sortie the tool rebuilds the timeline and reports, per sortie node:
- how armies ended (last action before removal) or that they were still in the field,
- whether the sortie target (kind,id) was changed, by which node / outside-tree code, into what,
  and whether the original target was taken up again later,
- who writes action/target/relay fields overall (unnamed bytes listed separately).
Writes <out>/summary.json and <out>/report.md."""
from pathlib import Path
from collections import Counter,defaultdict
import argparse,csv,json

# Codes observed at sortie (trace 2026-10-20) plus earlier research; unknown codes print as numbers.
ACTION={0:'없음',1:'이동',2:'아군 도시로 이동',3:'아군 관문으로 이동',4:'도시 공격',5:'관문 공격',6:'시설 공격',
        7:'부대 공격',8:'건설',9:'건설',11:'퇴각(11)',12:'퇴각(12)',13:'대기',16:'코드16'}
KIND={0:'없음',5:'도시',6:'관문',9:'지역(토지)',27:'부대'}
DECISION=('action','target_kind','target_id','target_hex','troops','relay1','relay2','relay3','relay4','relay5','facility','flags')

# Behaviour-tree nodes in game terms (tree position from REA_Analysis/behavior_trees_current.json,
# roles in research/ai_trace/원작_출진_목표변경_함수역할.md).
NODE_KO={'CCommandFootholdAttackDepatureNode':'군단 판단: 성 공격 출진','CCommandAreaOccupyDepatureNode':'군단 판단: 지역 점령 출진',
 'CDepartureNode':'군단 판단: 요격·건설 출진','CCommandTransportDepatureNode':'군단 판단: 수송 출진',
 'CCommandFrontFootholdTransportDepatureNode':'군단 판단: 전선 수송 출진','CDepartureTroopsCreateNode':'군단 판단: 출진 군세 편성',
 'CSpecifiedTroopsSetPurposeNode':'군세 목적 변경','CAttackFootholdRelayAreaAttackNode':'군세 판단: 성 가는 길의 지역 공략',
 'CExecuteFootholdOccupyNode':'군세 판단: 성 공격 명령','CExecuteAreaAttackNode':'군세 판단: 지역 점령 진행',
 'CExecuteInterceptNode':'군세 판단: 요격 대상 선택','CArmySetTargetArmyNode':'군세 판단: 행동에 맞춰 목표 지정',
 'CArmySetRetreatNode':'군세 판단: 임무 종료 귀환','CArmySetArmyActionNode':'부대 행동 지정','CSetRelayHexListNode':'중계 경로 지정',
 'CCommandExecutionFireStrategyNode':'계략(화계) 실행','BT 밖(일일 처리·전투 등)':'일일 처리·전투(행동 트리 밖)'}
def node_ko(n):return NODE_KO.get(n,n)
def date(d):
    if d is None or d<0:return '?'
    return f'{d//360}년 {(d%360)//30+1}월 {d%30+1}일'
def act(x):return ACTION.get(x,f'코드{x}')
def kind(x):return KIND.get(x,f'종류{x}')
def where(node,cause):return 'BT 밖(일일 처리·전투 등)' if cause=='between' else node
def median(v):v=sorted(v);return v[len(v)//2] if v else None

def main():
    ap=argparse.ArgumentParser();ap.add_argument('trace');ap.add_argument('--out',required=True)
    ap.add_argument('--keep-concurrent',action='store_true');a=ap.parse_args()
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    live={};episodes=[];unnamed=Counter();writer=Counter();troops_writer=Counter();transitions=Counter()
    node_calls=Counter();node_changes=Counter();departure_targets=Counter()
    concurrent=rows=0;mode='?';chained=set();first_day=last_day=None;preexisting=set();city_events=[]
    pending=None  # (episode or removal) collecting init.* lines
    with open(a.trace,newline='',encoding='utf-8',errors='replace') as f:
        for r in csv.DictReader(f):
            rows+=1
            if r['conc']=='1' and not a.keep_concurrent:concurrent+=1;continue
            cause,node,field=r['cause'],r['node'],r['field']
            day=int(r['day']) if r['day'].lstrip('-').isdigit() else -1
            if day>=0:first_day=day if first_day is None else min(first_day,day);last_day=day if last_day is None else max(last_day,day)
            if cause=='meta':
                if r['kind']=='mode':mode=field
                elif field=='chained_mod_hook':chained.add(node)
                continue
            if cause=='count':node_calls[node]+=int(r['result'] or 0);node_changes[node]+=int(r['new'] or 0);continue
            if cause=='city':
                if field=='owner':city_events.append({'day':day,'city':int(r['id']),'old':int(r['old']),'new':int(r['new']),'context':r.get('context') or ''})
                continue
            if cause=='call':
                if field=='departure_target' and r['new']!='0':departure_targets[(node,int(r['new']))]+=1
                continue
            old,new=int(r['old'] or 0),int(r['new'] or 0)
            if r['kind']=='troops':
                troops_writer[(where(node,cause),field)]+=1;continue
            aid=int(r['id']);commander=int(r['commander'])
            if field in('spawn','reuse','gone'):
                ep=live.pop(aid,None)
                if commander==0 or field=='gone':
                    # Army left the map: record how it ended (init.* lines give the last state).
                    pending={'removal':ep,'last':{}} if ep else None
                    if ep:ep.update(end_day=day,end_by=where(node,cause),end_context=r.get('context') or '')
                    continue
                if ep:ep.update(end_day=day,end_by=where(node,cause),end_action=None,replaced=True)
                ep={'army':aid,'commander':commander,'force':int(r['force']),'day':day,'by':where(node,cause),
                    'start':{},'changes':[],'context':r.get('context') or ''}
                episodes.append(ep);live[aid]=ep;pending={'start':ep};continue
            if field.startswith('init.'):
                if pending and 'start' in pending:pending['start']['start'][field[5:]]=new
                elif pending and 'removal' in pending and field=='init.action':pending['removal']['end_action']=old
                continue
            pending=None
            ep=live.get(aid)
            if ep is None:preexisting.add(aid);continue
            if field in DECISION:
                ep['changes'].append({'day':day,'by':where(node,cause),'field':field,'old':old,'new':new,
                                      'context':r.get('context') or ''})
                writer[(where(node,cause),field)]+=1
                if field in('target_kind','target_id'):
                    # kind and id of one change arrive as two lines from the same writer: count once.
                    group=(aid,day,cause,node,r['next'])
                    before=ep.setdefault('kind_now',ep['start'].get('target_kind'))
                    if field=='target_kind':ep['kind_now']=new
                    if ep.get('last_group')!=group:
                        transitions[(ep['by'],where(node,cause),before,ep['kind_now'])]+=1
                    ep['last_group']=group
            elif not field.startswith(('commander','hex','soldiers','morale','move_points')):
                unnamed[(where(node,cause),field)]+=1
    for ep in live.values():ep['alive']=True
    # Per sortie node: outcomes and target trajectories.
    by_node=defaultdict(list)
    for ep in episodes:by_node[ep['by']].append(ep)
    groups=[]
    for sortie,eps in sorted(by_node.items(),key=lambda kv:-len(kv[1])):
        outcome=Counter();first=Counter();first_days=defaultdict(list);restored=Counter();init=Counter();durations=[]
        for ep in eps:
            s=ep['start'];goal=(s.get('target_kind'),s.get('target_id'));init[(s.get('action'),s.get('target_kind'))]+=1
            if ep.get('alive'):outcome['기록 끝까지 야전']+=1
            else:
                outcome[f"제거: {act(ep.get('end_action'))}" if ep.get('end_action') is not None else '제거']+=1
                durations.append(ep['end_day']-ep['day'])
            cur=dict(kind=goal[0],tid=goal[1],action=s.get('action'));changed=None
            for c in ep['changes']:
                if c['field']=='target_kind':cur['kind']=c['new']
                elif c['field']=='target_id':cur['tid']=c['new']
                elif c['field']=='action':cur['action']=c['new']
                else:continue
                now=(cur['kind'],cur['tid'])
                if changed is None and now!=goal and goal[0] is not None:
                    changed=(c['by'],act(s.get('action'))+'/'+kind(goal[0]),act(cur['action'])+'/'+kind(cur['kind']))
                    first[changed]+=1;first_days[changed].append(c['day']-ep['day'])
                elif changed is not None and now==goal:restored[changed]+=1
            ep['target_changed']=changed is not None
        groups.append({'sortie':sortie,'armies':len(eps),
                       'initial':[{'action':act(k[0]),'target_kind':kind(k[1]),'count':v} for k,v in init.most_common()],
                       'outcomes':outcome.most_common(),'median_days_to_removal':median(durations),
                       'target_changed':sum(1 for e in eps if e['target_changed']),
                       'first_target_change':[{'by':k[0],'from':k[1],'to':k[2],'count':v,'median_days':median(first_days[k]),
                                               'later_restored':restored[k]} for k,v in first.most_common()]})
    summary={'mode':mode,'chained_mod_nodes':sorted(chained),'rows':rows,'concurrent_rows_skipped':concurrent,
             'days':[first_day,last_day],'sorties':len(episodes),'armies_present_before_trace':len(preexisting),
             'by_sortie_node':groups,
             'departure_targets':[{'node':k[0],'target':k[1],'count':v} for k,v in departure_targets.most_common(50)],
             'target_transitions':[{'sortie':k[0],'by':k[1],'from_kind':kind(k[2]),'to_kind':kind(k[3]),'count':v} for k,v in transitions.most_common(60)],
             'writers':[{'writer':k[0],'field':k[1],'count':v} for k,v in writer.most_common(200)],
             'troops_writers':[{'writer':k[0],'field':k[1],'count':v} for k,v in troops_writer.most_common(60)],
             'unnamed_bytes':[{'writer':k[0],'field':k[1],'count':v} for k,v in unnamed.most_common(60)],
             'node_calls':[{'node':k,'calls':v,'calls_with_changes':node_changes[k]} for k,v in node_calls.most_common()],
             'examples':sorted([e for e in episodes if e.get('target_changed')],key=lambda e:-len(e['changes']))[:25],
             'gameplay_interpretation_verified':False}
    (out/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    mode_text={'original':'원작만(모드 기능 꺼짐)','mod':'모드 기능 켜짐'}.get(mode,mode)
    L=['# AI 출진·야전 행동 추적 분석','',f'추적 모드: {mode_text}. 모드 훅이 연결된 노드: '+(', '.join(sorted(chained)) or '없음'),
       f'기간: 게임일 {first_day}~{last_day} ({(last_day or 0)-(first_day or 0)}일). 기록 {rows}줄, 출진 {len(episodes)}건, 추적 시작 전부터 있던 부대 {len(preexisting)}개(분석 제외). 다른 스레드와 겹친 기록 {concurrent}줄 제외.','',
       '## 출진 종류별 결과','']
    for g in groups:
        L.append(f"### {g['sortie']} — {g['armies']}부대")
        L.append('- 출진 때: '+', '.join(f"{i['action']}/{i['target_kind']} {i['count']}" for i in g['initial']))
        L.append('- 결말: '+', '.join(f'{k} {v}' for k,v in g['outcomes'])+(f" (제거까지 중앙값 {g['median_days_to_removal']}일)" if g['median_days_to_removal'] is not None else ''))
        L.append(f"- 출진 목표가 바뀐 부대: {g['target_changed']} / {g['armies']}")
        for c in g['first_target_change'][:6]:
            L.append(f"  - {c['by']}: {c['from']} → {c['to']} {c['count']}건, 출진 후 중앙값 {c['median_days']}일, 이후 원래 목표 복귀 {c['later_restored']}건")
        L.append('')
    L+=['## 야전 목표 전환 (종류 기준, 종류·ID 한 쌍을 1회로)','','| 출진 종류 | 바꾼 곳 | 전 → 후 | 횟수 |','|---|---|---|---|']
    L+=[f"| {t['sortie']} | {t['by']} | {t['from_kind']} → {t['to_kind']} | {t['count']} |" for t in summary['target_transitions'][:30]]
    L+=['','## 행동·목표·중계를 쓰는 곳','','| 쓴 곳 | 필드 | 횟수 |','|---|---|---|']
    L+=[f"| {c['writer']} | {c['field']} | {c['count']} |" for c in summary['writers'][:40]]
    L+=['','## 목표가 바뀐 부대 예시','']
    for ep in summary['examples'][:8]:
        s=ep['start']
        L.append(f"### 부대 {ep['army']} (무장 {ep['commander']}, 세력 {ep['force']}) — {ep['day']}일 {ep['by']}: {act(s.get('action'))} / {kind(s.get('target_kind'))} {s.get('target_id')}")
        for c in ep['changes'][:20]:
            v=(act if c['field']=='action' else kind if c['field']=='target_kind' else str)
            L.append(f"- {c['day']}일 {c['by']}: {c['field']} {v(c['old'])} → {v(c['new'])}")
        end=('기록 끝까지 야전' if ep.get('alive') else f"{ep.get('end_day')}일 제거({act(ep.get('end_action')) if ep.get('end_action') is not None else '?'})")
        L+=[f'- 결말: {end}','']
    # v12.13 traces carry a game-term context column: tell each army's story in game words.
    named=[e for e in episodes if e.get('context')]
    if named:
        named.sort(key=lambda e:(not e.get('target_changed'),-len(e['changes'])))
        L+=['## 부대 이야기 (게임 용어)','','각 줄의 오른쪽은 그 시점의 부대 상태입니다: 무장 | 세력 | 병력·사기 | 현재 위치 | 명령과 목표 | 잠정목표 | 적 접근 시 | 자주퇴각 | 추격 | 사후명령.','']
        for ep in named[:30]:
            L.append(f"### {date(ep['day'])} {node_ko(ep['by'])}")
            L.append(f"- 출진: {ep['context']}")
            groups_seen=[]
            for c in ep['changes']:
                key=(c['day'],c['by'])
                if groups_seen and groups_seen[-1][0]==key:
                    groups_seen[-1][1].append(c['field']);groups_seen[-1][2]=c['context'] or groups_seen[-1][2]
                else:groups_seen.append([key,[c['field']],c['context']])
            for (d,by),fields,ctx in groups_seen[:25]:
                L.append(f"- {date(d)} {node_ko(by)} [{', '.join(dict.fromkeys(fields))}] → {ctx}")
            L.append(f"- 결말: {'기록 끝까지 야전' if ep.get('alive') else date(ep.get('end_day'))+' 부대 해산 — '+(ep.get('end_context') or '')}")
            L.append('')
    if city_events:
        L+=['## 성·관문 소유 변화','']
        L+=[f"- {date(e['day'])} {e['context'].split('|')[0]}: 세력{e['old']} → {e['context'].split('|')[1] if '|' in e['context'] else e['new']}" for e in city_events]
        L.append('')
    summary['city_owner_changes']=city_events
    (out/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    L+=['## 참고','',"이름 없는 바이트 변경(분석 대상 아님) 상위: "+', '.join(f"{u['field']} {u['count']}" for u in summary['unnamed_bytes'][:8]),'',
        '행동·목표 코드는 출진 직후 값과 기존 조사로 붙인 이름이며 일부는 미확정입니다. BT 밖은 행동 트리 노드 사이의 변경(원작 일일 처리·전투 결과 등)입니다.']
    (out/'report.md').write_text('\n'.join(L)+'\n',encoding='utf-8')
    print(json.dumps({'status':'ai_trace_analyzed','rows':rows,'sorties':len(episodes),'report':str(out/'report.md')},ensure_ascii=False))
if __name__=='__main__':main()
