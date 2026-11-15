# 다른 제작자 모드 참고 색인

2026-10-15 지정 자료 상세 분석 갱신. **무술대회 v30 C 소스와 현재 11Patch 설치기를 우선 분석 자료로 추가**했다. 아래의 나머지 묶음은 2026-10-10 정적 조사 범위를 유지한다. 원본 참고 파일·게임 EXE·설치 DLL·공유 ZIP은 수정하지 않았다.

- 무술대회: [기능/상태 흐름·선택 UI·일기토·이벤트·차용 제약](references/MARTIAL_TOURNAMENT_V30.md). 소스의 완결된 함수 후보 58개를 검색 가능하게 추가했다. 원본 주소는 현재 보존 평문 프로필의 CALL 9곳과 모두 달라 재검증이 필요하다.
- 11Patch: [설치기 관리 코드·내장 payload·기능 근거·호환성](references/ELEVEN_PATCH_DEEP.md). 현재 파일은 63,900,672바이트, SHA-256 `eddb2c78…eb45673`이다. 이전 주 경로의 6,995,456바이트/`42d356b4…ae21048` 기록과 구분한다. 설명서 목록을 실제 구현 검증으로 대신하지 않는다.
- [대상별 지문과 조사 기록](../research/reference_deepening/analysis_memory.md). 기계 색인은 이 두 원본만 선택 갱신했다. 전체 `reference_mod`를 다시 훑은 결과가 아니다.

**새 기능마다 아래에서 관련 요약 하나만 읽는다.** 이미 우리 코드에 있는 기능은 [공통 함수](NATIVE_FUNCTIONS.md)와 [기능 지도](MODULES.md)를 먼저 사용한다.

| 요청 주제 | 읽을 짧은 요약 | 핵심 자료 |
|---|---|---|
| 무술대회·부대 없는 일기토·편집 이벤트·대진표·출전자 선택 | [무술대회 v30](references/MARTIAL_TOURNAMENT_V30.md) | `무술대회_코드_v30.c`, 함수별 줄/상태/미확인과 현재 지문 대조 |
| 11Patch 기능 차용·INI·설치/복원·내장 패치 | [11Patch 상세](references/ELEVEN_PATCH_DEEP.md) | 실제 설치기 CIL·내장 DLL/델타·소스와 각 근거 수준 |
| 공격·수비·귀환·출진·병참·AI 내정 | [AI](references/AI.md) | Editor_Source/ai + AI_AND_INTERNAL_AFFAIRS |
| 충성·부상·등용·친애·논공·관직 | [인사·관계](references/PERSONNEL.md) | 무장 객체 맵·memschema·merit 데이터 |
| 버튼·열·탭·마우스 설명·개성 표시 | [게임 화면](references/UI.md) | traits9ui/list·TraitHover·관계 탭 패키지 |
| 명품창고·분배·저장·시나리오 | [데이터·저장](references/STORAGE.md) | schema·treasuresync·LWC·파일 맵 |
| DLL 로더·후킹 충돌·업데이트·안개 | [로더·빌드](references/COMPATIBILITY.md) | gamebuild·패치 가드·안개 패키지 |

필요할 때만 원본 줄을 찾아본다. 게임 폴더에서:

```powershell
python -X utf8 AI_TacticsMod/tools/reference_index.py --query "명품|treasure" --scope Editor_Source/san14ce --limit 4
python -X utf8 AI_TacticsMod/tools/reference_index.py --query "ROOT_RVA|232EE0" --symbols --limit 4
python -X utf8 AI_TacticsMod/tools/reference_index.py --query "tour_start|sel_begin|msg_patch" --scope "무술대회_코드_v30.c" --symbols --limit 1
```

검색은 중복 내용을 제외하고 기본 최대 6파일×3줄만 출력한다. `--scope`로 폴더를 좁힌다. 과거 기록/시험은 필요한 경우만 `--all`을 추가한다. 정수 상수는 함수 주소로 자동 확정하지 않는다. 신규 자료를 넣으면 `--build`로 색인을 갱신하고 관련 요약에 근거를 추가한다.

기계 색인: `development/reference_inventory.json` (읽기 대상이 아니라 검색 도구용). **2026-10-15 선택 갱신 후 기록은 959개**, 고유 기록 지문 521개다. 무술대회 함수 후보 58개와 현재 설치기 지문을 반영했다. 다른 기록·ZIP 목록은 기존 관찰 시점 그대로이며 현재 폴더 전체의 새 집계가 아니다. 2026-10-10의 원래 조사 수치는 참고 파일 958개, 선언 2,360개, ZIP 17개(내부 ZIP 포함), 제외한 PyInstaller 환경 996파일이다. 그때 ZIP은 목록 중심으로 조사했으며, 이번 11Patch의 정적 추출은 상세 문서에서 별도로 설명한다.

| 묶음 | 분석 결과 / 향후 출발점 |
|---|---|
| Editor_Source + Modding_DOCS | 가장 유용. 파이썬 구현·함수 근거·레코드 스키마. 현재 게임과 일치하는 빌드 지문이 있음 |
| SAN14CE_Editor V7.7 / V8.3 | 실행 패키지·사용 설명. 내부 `_internal`은 파이썬 실행 환경. 우선 별도 Editor_Source를 사용 |
| San14EditorCE_0.11_KR | DLL/EXE + ko_editor.zip의 한글화 소스. 에디터 자체의 UI/언어별 파일 배치를 분석한 자료 |
| 4Mods v1.3 | 네 패키지가 같은 자료를 많이 공유. `01_포획대사/source`를 대표 경로로 사용. 포획·개성9칸·툴팁·상한200 |
| TraitHover / CaptiveDialog standalone | C 소스 있음. 툴팁 추적/플레이어 대사 판정·검증 가드 구조를 확인 |
| MeritTraits R15 / R17 | 배포 DLL·manifest·검사 기록 위주. R17은 변경 시 저장/규칙 캐시 갱신을 설명; 함수 구현 소스 없음 |
| RC25 Groups AutoSave | 출진 프리셋·그룹·자동저장 UI 참고. DLL·설명 위주; 기존 5훅 유지/객체별 사설 vtable 사용을 패키지가 설명 |
| RelationTab RC2.4 HoverPin | 무장 관계 탭·hover pin UI 참고. DLL·설명 위주; 6훅 트랜잭션을 패키지가 설명 |
| 무술대회 v30 | 읽기용 C 발췌본. 선발·대진·일기토·대사/선택 UI·보상 구현 확인, 단독 빌드 불가 및 현재 주소 불일치 구분 |
| 11Patch trial / (1) | 현재 주 경로는 이전 (1) 기록과 같은 63.9MB 지문으로 변경. 현재 파일의 관리 코드/내장 패키지 상세는 위 문서 사용; 과거 7MB 파일의 구현과 혼동하지 않음 |
| 전장의 안개 1.6 | 현 빌드 타임스탬프를 명시한 DLL 패키지. 구현 조사에는 기존 FogMod_Source와 문서의 2.4.5 분석을 구분하여 사용 |

**근거 수준:** 소스에서 읽음 / 제공 문서의 주장 / 우리 현재 게임에서 검증됨을 구별한다. 현재 EXE 해시가 같은 에디터도 모든 함수가 검증된 것은 아니다. 이전 빌드 주소, 다른 해시의 설치 DLL, 제공자의 합성 검사를 그대로 현재 게임 근거로 승격하지 않는다.
