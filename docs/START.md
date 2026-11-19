# 작업 시작

작업 전에 [세션 공통 규칙](SESSION_RULES.md)을 읽고 그대로 따른다. 이어서 [현재 설치·배포 상태](../development/CURRENT.md)를 읽는다. 버전·해시의 기준은 그 파일과 manifest다. 일반 사용자는 [모드 전체 설명](../README.md)을 읽는다.

## 개발 시 읽을 순서

1. SESSION_RULES → CURRENT → [MODULES](MODULES.md)의 관련 행 → 해당 소스·[NATIVE_FUNCTIONS](NATIVE_FUNCTIONS.md). 그 밖의 문서는 필요할 때만 연다.
2. 새 기능을 설계하기 전: [모드 개발 시행착오](MOD_DEV_LESSONS.md), [새 기능 규칙](FEATURE_RULES.md). 기능을 추가·수정·삭제할 때는 [기능 추가·수정 필수 체크리스트](FEATURE_CHANGE_CHECKLIST.md)를 따른다.
3. UI를 고칠 때: [UI 실패 기록과 해결 방식](UI_FAILURES_AND_RECIPES.md)의 증상표, OFF/해제 뒤 버튼이 남으면 [UI 리소스 복원 기준](UI_RESOURCE_LIFECYCLE.md). 이미 실패한 방식(버튼 생성 시점·선택 모드·전체 저장·내부 클리핑·외교 퇴장)으로 되돌리지 않는다.
4. 함수 조사 범위는 [FUNCTION_AUDIT](../development/FUNCTION_AUDIT.md)(자동 색인은 개별 검증이 아님), 외부 근거는 [REFERENCE_MODS](REFERENCE_MODS.md)에서 하나만 고른다.
5. 검사·배포는 [WORKFLOW](WORKFLOW.md)·[PUBLISH](PUBLISH.md), 리팩토링 뒤 실게임 한 번 확인은 [실게임 최소 확인](RUNTIME_SMOKE.md), 다른 모더 호환은 [MODDER_COMPAT](MODDER_COMPAT.md).

서브에이전트 작업자는 [AGENT_WORKER](AGENT_WORKER.md)와 작업서의 허용 범위·반환 형식을 따른다.

## 작업 중 문서 갱신

- 확인된 원인·결정·변경 파일·검사 결과·미확인 범위를 짧게 기록한다. 사용자 요청과 상태는 `development/USER_REQUESTS.md`, 설치 상태는 `CURRENT.md`(생성기)에 둔다.
- 실제 게임 실행·설치·배포를 하지 않았으면 그렇게 적고, fixture·정적 검사·빌드 결과를 실게임 검증과 구분한다.
- 이 문서에는 장기 규칙과 읽기 순서만 둔다.

## 현재 유지할 결정

- 숙련 설정과 계수·툴팁은 편의 DLL 전용.
- 공성 잠금은 실제 적 성/관문 피해와 30일 정지 집단 진군 두 경로. 일반 40일 정지 귀환 유지. 건설 횟수 제한·성 앞 공격 우선은 제거됨.
- 사용자 결정(2026-10-21, `combat.mission_v2`): 적 성·관문을 공격 중인 부대는 40일 정지 퇴각에서 제외(최종 목표는 정지 퇴각 없이도 막히지 않는 AI). 건설은 목표 성 8칸 안 공성 시설 1개, 건설 대기 없음. 3천 이하 부대는 원작 퇴각. 출진은 원작 판단, 4만 이상만 대열 분할.
- 지원·내정 군단 출진 제한은 전투 DLL 없이도 쓴다. 두 DLL의 1EC890 소유권은 중복하지 않는다.
- 구버전 dist 파일은 덮어쓰기·삭제 금지. 현재 패키지에는 전법 설명 TXT를 넣지 않는다.
- 문서만 바뀌면 DLL 재빌드·설치 불필요.
- 설치 담당(사용자 2026-11-20 「설치는 이 세션이 담당하자」): 로컬 설치는 사용자가 정한 설치 세션 하나만 한다(지정: 「mymod 버전 확인」 세션). 다른 세션은 검사·커밋까지 하고 san14-handoff의 `handoff draft:` 줄을 사용자나 설치 세션에 넘긴다. 설치 세션은 커밋된 변경으로 바뀐 모듈을 모두 다음 local 번호로 설치하고, 버전 번호도 설치 세션이 정한다.
- 구버전 백업·후보·중복 작업본은 backup의 역사 보관 ZIP과 색인에 있다. 필요한 경로를 복원한 뒤 과거 근거로만 쓴다. 지운 문서와 옛 빌드 자료 위치는 [HISTORY](HISTORY.md).
