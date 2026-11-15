# AI·내정 참고

원본 분석과 다른 모드의 정책 구현을 구분한다. 다음 자료를 우리 `native_campaign/facilities/domestic/adjacency`에서 이미 조사한 경로와 대조한다.

| 기능 | 원본 자료와 시작 위치 | 얻은 내용 |
|---|---|---|
| 전략 목표/플레이어 판정 | [AI 조사](../../../reference_mod/Modding_DOCS/memory/AI_AND_INTERNAL_AFFAIRS.md), §2(119줄) | 목표 배열 `+0x17A`; 군단 슬롯→마스터 세력→플레이어 구분 |
| 목표 편향 | [playerfocus.py](../../../reference_mod/Editor_Source/ai/playerfocus.py) | 실제 목표 목록을 재정렬하는 정책. CKYai의 ‘플레이어 중시’ 이름과 실제 후킹 의미가 다르다는 정정 |
| 출진 병력 | [deploymin.py](../../../reference_mod/Editor_Source/ai/deploymin.py), [deployfloor.py](../../../reference_mod/Editor_Source/ai/deployfloor.py), [deploysplit.py](../../../reference_mod/Editor_Source/ai/deploysplit.py) | 병력 계산 시작값 변경과 실제 출진 하한은 다른 경로. split 머리말은 시작값 훅이 턴당 0~2회여서 효과가 드물었다고 기록 |
| 귀환 | [retreat.py](../../../reference_mod/Editor_Source/ai/retreat.py):1–27 | 부대 `+0x37=0x0B`, `+0x38=5`, `+0x3A=거점번호`를 쓰는 외부 루프. 반경5·전력비1.6은 **참고 모드의 정책**, 원작 AI 공식으로 확정한 것이 아님 |
| 명령서 | [cmdrefill.py](../../../reference_mod/Editor_Source/ai/cmdrefill.py) | 세력별 명령서 보충. 우리 지역 담당관 임명만 무료화하는 요구와 적용 범위가 다름 |
| 내정 경제/병량 | [upkeep.py](../../../reference_mod/Editor_Source/ai/upkeep.py), [supply.py](../../../reference_mod/Editor_Source/ai/supply.py), [steward.py](../../../reference_mod/Editor_Source/ai/steward.py) | 별도 등록 담당관·정치 기반 감면 루프. 게임 원래 지역담당관 자동개발 로직과 혼동하지 않음 |
| 훈련·사기·외교 | [training.py](../../../reference_mod/Editor_Source/ai/training.py), [morale.py](../../../reference_mod/Editor_Source/ai/morale.py), [diplomacy.py](../../../reference_mod/Editor_Source/ai/diplomacy.py) | 확장 정책 계산·관계 값 접근 후보. 기능 요청에 맞는 파일만 읽음 |
| 피해 세력 구분 | [dmgside.py](../../../reference_mod/Editor_Source/ai/dmgside.py) | 공격측/방어측 세력 유도와 조건별 피해 배율. 지역 인접 버프 함수와 동일하다고 가정하지 않음 |

공통 데이터 후보: `image+0x277DF18` 루트, `root+0x871E8` 게임 객체, 게임 `+0x3A` 플레이어 마스터 ID, `root+0xE0D0` 군단 표. 무장 `+0xE0`는 마스터 세력 ID가 아니라 군단 슬롯이다. 지도는 `root+0xE270`, 220×220, 표 stride `0x20`, 소유 `+0x14`, 거점 `+0x1A`라는 근거가 있다. **사용 전 우리 현재 profile과 대조**한다.

주소 예(제공 문서 §1.3): 시작 병력 즉치 `0x7A3C9→0x7AF99`, 목표 목록 쓰기 `0x57D60→0x58160`, 유효 전력 평가 반환 `0x83A8C→0x57F8C`. 앞=09-10, 뒤=09-28; 마지막처럼 함수가 통째로 옮겨져 일정 오프셋 가산으로 이식할 수 없다.

참고 소스는 외부 메모리 쓰기·코드 케이브 방식이 많다. 알고리즘·필드 근거만 가져오고 우리 정상 AI 콜백에 맞춰 설계한다. 실행하지 않았다.

성 주변 점령→건설→추가 점령, 병참단절 우선, 토성/석벽 금지의 현재 구현은 **우리 소스가 우선**이다. 이 참고 자료에서 그 전체 행동 순서를 확인한 것은 아니다. 조사 문서 §7(453줄)의 ‘지역담당관 자리 미확정’ 등은 당시 미해석 목록으로 보존한다.
