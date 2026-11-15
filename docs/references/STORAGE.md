# 명품·파일·저장 참고

미구현 명품창고/분배/외교의 데이터 조사 출발점이다. **파일 포맷을 아는 것과 게임의 수여·외교·효과 갱신 함수를 아는 것은 별개**다.

| 자료 | 얻은 내용 |
|---|---|
| [schema.py](../../../reference_mod/Editor_Source/san14ce/schema.py):727–770 | CE 명품 레코드69B, `treasure_fields()`로 능력/효과 정의를 공유. `end=202`의 최신 스키마 |
| [treasuresync.py](../../../reference_mod/Editor_Source/san14ce/treasuresync.py):1–35 | 공용 정의→시나리오 명품65B 투영. 소유/위치/상태는 보존하고 이름·효과·CG만 복사 |
| [명품 파일 맵](../../../reference_mod/Modding_DOCS/format/SAVE_CITY_FACTION_MAP.md):220–291 | 최초 소유 필드/종류 조사 및 한 레코드 어긋남 정정. 초기에151개를 관측했다는 기록; 슬롯 상한은 최신 스키마와 대조 |
| [treasure_abilities.json](../../../reference_mod/Editor_Source/san14ce/refs/treasure_abilities.json) | 0무효, 1통솔, 2무력, 3지력, 4정치, 5매력. 매력과 충성 수여 효과를 혼동하지 않음 |
| [treasure_effects.json](../../../reference_mod/Editor_Source/san14ce/refs/treasure_effects.json) | 0무효, 1퇴각확실, 2수명연장. 능력 종류와 특수 효과를 각각 분류해야 함 |
| [LWC_FORMAT](../../../reference_mod/Modding_DOCS/format/LWC_FORMAT.md), [lwc.py](../../../reference_mod/Editor_Source/san14ce/lwc.py), [lwc_enc.py](../../../reference_mod/Editor_Source/san14ce/lwc_enc.py) | 파일 컨테이너/압축 구조, 해제·재압축 구현.다른 언어/PK/CE의 레코드 배치를 섞지 않음 |
| [FIXDATA_SIGNATURE](../../../reference_mod/Modding_DOCS/format/FIXDATA_SIGNATURE.md), [masterschema.py](../../../reference_mod/Editor_Source/san14ce/masterschema.py) | 마스터 파일 서명·필드 정의. 일반 세이브와 동일 규칙으로 저장하면 안 됨 |
| [scenschema.py](../../../reference_mod/Editor_Source/san14ce/scenschema.py), [savefile.py](../../../reference_mod/Editor_Source/san14ce/savefile.py) | 파일 종류별 별도 스키마·변경 추적·저장 경로 |

소스에 명시된 시나리오 명품 오프셋: `+0x00 소유`, `+0x02 소재`, `+0x04 상태`, `+0x05 양도`는 보존; `+0x3A`부터 종류·능력·능력값·효과·lp46·가치. **실행 중 메모리 구조로 그대로 사용하지 않는다.**

사용자 요구를 위한 후속 조사: 능력별 최고1개/명마·수명 분류, 원작 수여·회수의 충성/효과 갱신, 군주 사망/무장 포로·이적, 창고 소유와 세력 연결, 외교 목록의 미분배 필터, 저장 재시작. 이 참고 자료만으로 창고 기능이 완성된 것은 아니다.

논공 친애의 별도 저장은 현재 우리 `native_merit_save.inl`의 형식·검증이 우선이다. R17은 ‘변경 없으면 JSON 직렬화 생략·저장 해시 재사용’을 설명하지만 해당 구현 소스/실게임 검증은 제공하지 않는다.

San14EditorCE_0.11_KR 안 `ko_editor.zip/ko_editor/README.md`·`build_ko.py`는 MFC **에디터**의 문자열/메뉴/대화상자 한글화와 언어별 로더/라이터 길이를 다룬다. `.kor` 섹션·참조 기반 문자열 치환 설계는 참고 가능하지만 그 RVA는 게임 주소가 아니다. 현재 펼친 파일 검색 대상에는 포함되지 않으며 ZIP 텍스트를 직접 읽는다.
