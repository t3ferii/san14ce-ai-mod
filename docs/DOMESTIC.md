# 자동 내정과 1군단 범위

`SAN14_Convenience.dll`의 `native_domestic.inl`이 담당한다. `[groups] convenience`와 `[domestic] domestic`을 켜면 컴퓨터 세력·플레이어의 2군단 이후에 균형 개발을 적용한다.

| 대상 | 균형 개발 | 자동 임명 명령서 비용 0 | 명품 AI 배분 |
|---|---|---|---|
| 컴퓨터·플레이어 위임 2군단~ | domestic 켜짐 | domestic + domestic_free 켜짐 | awards + awards_ai 켜짐 |
| 플레이어 직할 1군단 | domestic + domestic_direct 켜짐일 때만 | 적용 안 함 | 적용 안 함 |
| 플레이어 수동 임명 | 자동 균형 개발 대상과 별개 | 적용 안 함 | 해당 없음 |

`domestic_direct` 기본값은 0이며 원작 자동내정 옵션(3329B0)을 켰는지로 결정하지 않는다. 이 옵션을 켜면 1군단의 직접 정한 개발 항목도 매일 균형 개발로 바꿀 수 있다. 1군단 비용은 이 옵션과 무관하게 항상 원작 경로를 사용한다.

균형 개발은 MAX를 제외한 가장 낮은 개발 레벨을 선택하며 동률이면 상업→농업→병영 순서다. 이미 임명된 담당관도 실제 내정 틱에서 갱신한다. 예상치 조회는 변경하지 않는다. 자동 임명 비용은 같은 AI 노드의 TLS 범위에서만 0으로 처리하며 직접 명령의 공통 실행기를 전역 무료화하지 않는다.

부상 후보·명품 배분·본초학은 같은 틱에 있어도 별도 스위치와 대상 검사를 사용한다. `population`은 별도 월간 인구 증가 4배다. 지역담당관 초상화/개성은 [표시 전용 기능](REGION_OFFICERS.md)이다.

근거: `src/native_domestic.inl`, `src/native_awards.inl`, `src/native_adapter.cpp`, `src/native_split_install.inl`, `distribution/FeatureSelect.ps1`. 검사: `check_feature.ps1 -Feature domestic -List`; 공통 연결 변경은 core/dll-split 추가. 실제 각 군단의 진행 결과는 파일/fixture 검사와 구별한다.
