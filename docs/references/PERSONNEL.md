# 인사·관계·부상 참고

우선 우리 `native_personnel`, `personnel_policy`, `native_merit`, `NATIVE_FUNCTIONS`의 검증 기록을 사용한다. 추가 조사 자료:

| 대상 | 원본 위치 | 재사용할 근거 |
|---|---|---|
| 관계/충성/관직 | [AI 조사 §5](../../../reference_mod/Modding_DOCS/memory/AI_AND_INTERNAL_AFFAIRS.md):370–440 | CPersonData 객체·인사 함수 맵, 09-10/09-28 주소 비교 |
| 실행 중 레코드 | [memschema.py](../../../reference_mod/Editor_Source/san14ce/memschema.py) | 스탯 앵커 기준과 객체 기준 차이, 진짜 이름 필드/캐시 구분 |
| 세이브 무장 | [schema.py](../../../reference_mod/Editor_Source/san14ce/schema.py):450–560, [세이브 맵](../../../reference_mod/Modding_DOCS/format/SAVE_GENERAL_MAP.md) | 관계·상병·건강 등 파일 필드. 메모리 오프셋과 섞지 않음 |
| 상병/정책 ID | [injuries.json](../../../reference_mod/Editor_Source/san14ce/refs/injuries.json), [policies.json](../../../reference_mod/Editor_Source/san14ce/refs/policies.json) | 상병 0건강/1경증/2중증/3빈사, 본초학 등의 이름표 |
| 논공 원본 효과 | [merit_effects.json](../../../reference_mod/Editor_Source/san14ce/refs/merit_effects.json), [fixschema.py](../../../reference_mod/Editor_Source/san14ce/fixschema.py) | 원래 논공 효과 종류와 보상 정의 표 |
| 다른 논공 모드 | [R17 manifest](../../../reference_mod/SAN14CE_MeritTraits_R17_Install/SAN14CE_MeritTraits_R17_Install/package.manifest.json), [변경내역](<../../../reference_mod/SAN14CE_MeritTraits_R17_Install/SAN14CE_MeritTraits_R17_Install/변경내역.txt>) | 개성 부여·재추첨/저장 최적화 설명. 우리 논공 군주 친애와 다른 기능; 구현 소스 없음 |

문서의 **객체 기준** 후보:

| 필드 | 오프셋 / 폭 |
|---|---|
| 배우자 / 의형 그룹 대표 | `+0xBA u16` / `+0xBC u16` |
| 친애 / 혐오 | `+0xC0 u16×8` / `+0xD0 u16×8` |
| 소속 군단 슬롯 / 위치 / 신분 | `+0xE0 u8` / `+0xE4 u16` / `+0xE6 u8` |
| 관직 / 생 충성값 | `+0xE7 u8`(73없음) / `+0xE8 u8` |
| 공적등급 / 공적 | `+0x1BC u8` / `+0x1BE u16` |

인사 함수 후보(09-28): 친애 `0x232EE0`, 혐오 `0x232A00`, 혈연·의형 `0x214490`, 지휘병사 `0x2B2920`, 임명 충성 변화 `0x1EDA60`, 임명 루프 `0x1F1750`, 공적 가산 `0x1DE2A0`.문서의 이름만으로 정확한 ABI·호출 조건을 확정하지 않는다.

**정정/주의할 실제 차이:**

- `memschema.py:172`는 생 충성을 1바이트로 읽지만 편집 상한은 `hi=100`이다. 우리 100초과 표시에는 생 값을 쓰며 이 에디터의 입력 제한을 복사하지 않는다.
- 스탯 앵커는 객체 `+0xEC`이다. 따라서 에디터의 `충성 -0x04`가 객체 `+0xE8`와 같다. 레코드 이름·메타는 이전 레코드 영역에서 읽는 구조다.
- `injuries.json` 설명의 세이브 `+0x17E`는 PK 조사이며, CE `schema.py:547`은 `+0x187`을 선언한다. 상병 등급 이름표와 실제 메모리 위치를 별도로 검증한다.
- 조사 문서 §5.2는 일부 폭 오류·‘외부 heap에 있다’는 오래된 설명을 정정한다. 소스에도 그 전 설명이 일부 남아 있다. 일반화한 문장보다 필드별 증거와 현재 값을 우선한다.
- 배우자·의형은 양방향/그룹 일관성이 필요하다. 등용 권고 예외·논공 별도 친애의 실제 전투/연계 처리는 우리 구현과 원작 판정 경로에서 확인한다. 이 자료가 요구 기능 전체를 구현한 것은 아니다.
- 사관금지 `memschema.py:181..187`: 객체 `+162 u16`은 금지 군주 무장번호다. 기간 `+164`의 참고 메모리 스키마 u16 설명은 현재 원본 `2390B0` 저장 폭 **u8**과 다르다. 원본 `2784B0`는 현재 등용 군주 번호만 비교하며 기간을 별도 검사하지 않는다. [우리 확인 기록](../../research/v8/등용가능성_친애와사관금지.md)을 우선한다.
