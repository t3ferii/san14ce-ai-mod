# AI 수정 지침

코드의 `AI-CONTRACT [분류]` 주석은 구현을 바꿀 때 함께 보존·검증할 제약이다. 주석은 현재 소스 관찰에 근거하며 새로운 ABI 검증이나 실게임 검증을 대신하지 않는다.

| 수정 대상 | 함께 확인할 계약·검사 |
|---|---|
| 공통 어댑터·설정 | `native_adapter.cpp`의 build variant, 통합 INI/legacy fallback, 설치기 카탈로그와 기능별 부모 AND 조건 |
| DLL 경계·훅 | `split_module_api.h` 필드/호출 규약/크기, `native_split_install.inl` 소유권·feature guard·부분 게시 실패 원복; `dll-split` 경로 |
| 출진·이동 | `native_sortie.inl` 포인터 인수, native 검색 재진입, 플레이어 명령 소유권; `sortie`와 campaign 관련 fixture |
| 공성·정지 회군 | 실제 적 성/관문 피해 잠금, 30일 정지 집단 진군의 자체 이동만 허용, 일반 40일 정지 퇴각, 같은 날 경쟁 작성자·목표 점령·객체 ID 재사용 |
| 경로 가속 | 편의 DLL의 183A30/184485와 전투 DLL의 1843E0 소유권, native slot당 storage 수명, waypoint마다 깨끗한 탐색, 원작 위임 시 상태 복구; `path_fast_fixture.cpp` |
| 저장 | `native_save_codec.inl` roundtrip 검증·원작 fallback·할당자 일치, `native_merit_save.inl` epoch·원작 save binding·호스트 스레드 |
| UI | 예상 피해량은 표시만, 건물파괴는 성공한 원작 1차 처리 후 확장, 숙련 행은 layout 수명·commit/cancel/save 의미 보존 |
| 외부 payload | 고정 지문, unwind 등록, 실행/데이터 섹션 보호와 출처 유지; `THIRD_PARTY.md` |

수정 전에는 MODULES의 관련 행에서 진입하고, 바꾸는 함수와 호출자만 읽는다. `.inl`이 어느 DLL에서 포함되는지 전처리 조건까지 추적한다. 함수 이름의 유사성이나 fixture 통과만으로 원작 주소·ABI가 맞는 것으로 판단하지 않는다.

동작을 바꾸면 사용자 요구를 고정한 fixture를 추가하고 기존 검사를 약화하지 않는다. 기능 검사 후 전체 split 검사까지 수행하며, 누락된 게임/외부 자료 때문에 검사를 못 했으면 그대로 기록한다. 문서·주석만 바꿀 때는 실행 토큰 동일성과 링크·경로를 검사한다. 실게임 입력·장시간 안정성 확인은 별도다.
