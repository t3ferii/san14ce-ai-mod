# 원작 이름 탐색 E09910 가속

현재 분리 배포에 포함된 편의 DLL 기능이다. `[performance] name_lookup`과 `progress_native.ini [name_lookup] enabled`를 함께 켜야 한다. 설치 해시는 [CURRENT](../development/CURRENT.md)가 기준이며 아래 v11.31 해시는 과거 측정 구성이다.

순수 원작 WPR 기록에서 턴 경계 로직 스레드 샘플의 30~50%를 차지한 `E09910`(핫 조각 `E09948`)을 결과 동일한 탐색으로 바꾼다. 근거: `REA_Analysis/perf_20261018/README.md` 발견 1, `data/performance/vanilla_20261018_functions.json`.

## 원작 의미 (현재 EXE 바이트·REA 확인)

- `E09910(RCX table, RDX key)`: `[table+8]`이 0이 아닐 동안 `table=[[table+8]+0x20]`. 이후 `[table]`의 0x58바이트 레코드 `[table+0x18]`(u32)개를 앞에서부터 보며, 매번 `strlen(record)`·`strlen(key)`를 계산하고 `102FC20(record,key,max(두 길이))`가 0이면 그 레코드 포인터를 반환, 끝까지 없으면 0.
- `102FC20` = CRT `strncmp`(0 또는 부호 없는 바이트 비교 결과 ±1). REA `ev_32bb6c95…`(perf.pe). E09910은 0 여부만 보므로 **정확한 문자열 일치**, 첫 일치 레코드 반환.
- 개수 0이면 key를 읽지 않는다. 함수 길이 0xCE, `+144` CALL rel32→102FC20.

## 구현 (`SAN14_Convenience.dll` 단독 소유)

- `src/name_lookup.h`: 순수 커널. 같은 부모 체인·순서·첫 일치. 첫 바이트 비교 후 불일치/NUL에서 멈추는 비교. 원작이 읽는 범위 밖 바이트를 읽지 않는다. 캐시·잠금·전역 상태 없음.
- `src/native_name_lookup.inl`: E09910 전체 0xCE바이트 + 102FC20 0x7D바이트 가드. 첫 명령 `MOV [RSP+10],RBP`(5바이트)를 `JMP rel32 → FF25 relay → name_lookup_entry`로 16바이트 원자 교체. 호출한 게임 스레드에서 그대로 실행(원격 호출·실행 중 메모리 쓰기 도구 없음). 가드 불일치·실패 시 원작 유지, 다른 기능 영향 없음.
- 호출 시점: `split_activate`에서 `install_progress_native()` 다음. 헥스 경로/거리 가속(`native_progress.inl`, 2AA850·43E030, 자체 relay)과 블록·relay·호출 경로가 겹치지 않는다.
- 스위치: `AI_TacticsMod/progress_native.ini`의 `[name_lookup] enabled=1|0`(시작 시 1회 읽음, 기본 1). A/B 비교용.
- 2단계(미구현): 배열별 이름→첫 인덱스 캐시. 배열 내용 변경을 싸게 감지할 근거가 없어 보류. 1단계 측정 후 판단.

## 검사 (`tools/check_feature.ps1 -Feature name-lookup [-Build]`)

- `name-lookup`: `tests/name_lookup_fixture.cpp`가 해제본 `build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe`의 **원작 E09910·102FC20 기계어를 비공개 버퍼에서 실행**해 새 커널과 비교. 실행 중 캡처 `.text`(`running_code_text.bin`)와 가드 일치, CALL 대상 확인. 무작위(작은 알파벳·고바이트·최대 길이·접두/확장·빈 표·부모 체인 0~3단계·미끼 표·레코드 내부 key), 원작 `.rdata` 실제 문자열 20,000건 포함 총 87,248건 불일치 0. 결과 `build/name_lookup_fixture.json`.
- 미세 측정(3,000레코드·2,000질의, 실제 문자열): 원작 약 195ms → 약 8.5ms(약 22배). 게임 내 단축 폭이 아니다.
- `split-module` + 개발 빌드(`build/development/loader_split`) 통과. 설치·Apply·ZIP 변경 없음. 당시 개발 단계 기록이며, 이후 A/B 측정은 아래 별도 결과를 따른다.

## 과거 설치·측정 구성 (2026-10-18)

- 관리.ps1 Apply(guarded)로 설치. 1차(빈 전투 모듈 `21bc0571…`) 후, 같은 날 `[groups]` 묶음 스위치를 넣은 `v11.31-groups-name-lookup`으로 갱신: dinput8 `4170641a…`, SAN14_CombatAI `7d9928a7…`(설치되지만 `combat=0`이라 로드 안 됨), SAN14_Convenience `475293b8…`. 게임 폴더 해시 일치 확인.
- 사용자 요청으로 전투 판단 제외(`[groups] combat=0`), `[features] unified=0`으로 무장 검색·통합 인사 제외(전법 자동 선택도 함께 꺼짐). 공적·개성9·부상 등 나머지 편의 기능과 `performance`(거리 가속+이름 탐색, `[name_lookup] enabled=1`)는 켜짐.
- 이 설치 단계에서 이전에 준비되어 있던 v11.31 스테이징(`build/input_package`, 미설치)을 현재 개발 빌드로 덮었다. 이 과거 staging을 현재 패키지 위에 다시 만들지 않는다.
- 실게임 실행·훅 적용 여부는 아직 미확인(배포 빌드는 로그를 쓰지 않으므로 B 기록에서 `original_E09910` 샘플 0으로 확인).

## 실게임 A/B 결과 (2026-10-18, 같은 세이브, 각 턴 경계 5회)

설치 구성 동일(`combat=0`, `unified=0`, `performance=1`), `[name_lookup] enabled`만 0(A)/1(B). 기록 `data/performance/name_lookup_A_20261018_141426.etl`(PID 23088), `name_lookup_B_20261018_141915.etl`(PID 39468), 결과 `data/performance/name_lookup_ab_20261018.json`. 로드 기준 둘 다 `0x7FF65F910000`.

| 항목 | A(원작 탐색) | B(교체) |
|---|---|---|
| 스택에 원작 `E09910` 포함 샘플 | 14,865 | **0** (훅 실제 적용 확인) |
| 호출자 `8A8B80` 포함 샘플 합계 | 14,153 | 3,108 (교체 함수가 잎인 샘플 2,245) |
| 턴 경계당 `8A8B80` 샘플(평균) | 2,815 | 606 (**약 −78%**) |
| `8A8B80` 활동 구간 길이(초, 평균) | 19.8 | 13.2 |

- 턴 경계당 이 경로의 CPU가 약 2.2초 줄었다(샘플≈ms). 활동 구간 길이도 짧아졌지만 이는 1초 단위 CPU 활동 범위이며 클릭~조작 가능 시점의 벽시계 턴 시간이 아니다. 정확한 체감 단축은 스톱워치 측정이 필요하다.
- B 실행에서 5회 턴 경계 동안 충돌 없음(짧은 실행 1회 관찰, 장기 안정성·저장 재로드는 미확인).

## 동일 세이브 A/B CPU 측정 방법

관리자 권한 필요: `optional_tools/AI모드_이름탐색AB측정_관리자.cmd A|B` (`tools/name_lookup_trace.ps1`, 게임 종료 시 자동 정지).

1. 사용자 승인 후 이 개발 빌드를 guarded 설치 도구로 설치(게임 종료 상태). 두 실행은 **같은 모드 구성**에서 `[name_lookup] enabled`만 0(A)/1(B)로 바꾼다(모드 미설치 vs 설치 비교는 다른 훅이 섞여 무효).
2. 같은 세이브를 불러와 같은 턴 수(예: 4턴)를 같은 방식으로 진행. 각 실행을 `REA_Analysis/perf_20261018/scripts/vanilla_trace.ps1`과 같은 방식(`wpr -start distribution\SAN14CpuSample.wprp!SAN14CPU -filemode`)으로 기록. 가능하면 각 턴 종료 클릭~다음 조작 가능 시점을 스톱워치로 함께 기록.
3. PID와 로드 기준 확인: 스레드 시작 `SAN14CE_KO.exe!X` → 기준 = X−0x1031448.
4. `xperf -i run.etl -o run.csv -a dumper` 후
   `python tools/compare_name_lookup_ab.py --a-csv A.csv --a-pid … --a-base … --b-csv B.csv --b-pid … --b-base … --out data/performance/name_lookup_ab.json`
   자체 검사(원본 기록 vanilla_cpu_20261018_130227.etl을 A=B로): 프로세스 461,481샘플, 스택에 `E09910` 11,921·`8A8B80` 10,713(E09910은 다른 호출자도 있음), 100샘플 이상 버스트 4개(45·81·109·138초 시작, 평균 2,640샘플). CSV는 약 4GB이므로 임시 폴더에 만들고 끝나면 지운다.
5. 판정: 호출자 `8A8B80` 포함 샘플/턴 경계(버스트) 비율, 버스트 길이(초), `original_E09910` 샘플이 B에서 0인지(훅 적용 확인), 스톱워치 턴 시간. CPU 감소와 벽시계 단축은 따로 보고한다.
