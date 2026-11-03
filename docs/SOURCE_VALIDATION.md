# v15.2 소스 공개 검증 (2026-11-03)

- 현재 공개 무로그 빌드 소스 405개 파일을 내보냈습니다. 원본과 공개본의 C/C++ 실행 토큰 동일성과 20개 AI-CONTRACT 주석을 검사했습니다.
- 새 조조전 설정창·대표 그림·제목 버튼 소스와 결정/취소/레이아웃 검사를 포함합니다. 개인 경로만 가리고 사용자 설정·게임 자료·로그·백업·바이너리는 제외했습니다.
- 로컬 전체 분할 회귀 검사, 공개/진단 빌드, 설치기 정적451·설정 왕복596 검사 통과. 편집기 진단/일반 모의 검사 각2446, 원작 바이트/공용 창 검사52, 조조전 설정 정적55, 화면 적용/복구27 검사 통과.
- 로컬 진단판 설치와 설정·해시·내보내기 검증 완료. 이번 UI 실게임 확인은 아직 남아 있습니다. 공개 저장소 단독의 편의 DLL 전체 빌드는 외부 입력이 필요합니다.

## 이전 소스 검증 기록

# 소스 공개 검증 (2026-11-01, v15)

- v15 배포를 빌드한 작업 폴더에서 `tools/export_source_snapshot.py`로 381개 파일을 옮겼다. 이전 공개(2026-10-26)에 빠졌던 설치기 소스 `installer/InstallerGui.cs`, 조조전 DLL(`src/ccz_module.cpp`·`ccz_module_api.h`·`native_ccz_bridge.inl`·`native_ccz_sprites.inl`), 지형 편집·격류 물살·나무 소스와 해당 검사를 포함한다.
- 핵심 소스의 AI-CONTRACT 주석 20개를 유지했다. 원본과 공개본의 C/C++ 파일에서 문자열/문자 리터럴을 보존하고 주석·공백만 제외한 토큰 열이 동일함을 검사했다. 게임 로직의 실행 토큰을 바꾸지 않았다.
- 개인 PC 경로는 `C:\Users\<user>\...`로 가렸고 capstone 검색 경로 줄은 `pass`로 바꿨다. Python 81개 파일 문법과 자격 증명 패턴을 검사했다.
- 공개 폴더에서 `tools/build_public_smoke.ps1 -Combat -Ccz`를 실행해 종료 코드 0을 확인했다. 로더 dinput8.dll, 실제 `SAN14_CombatAI.dll`, `SAN14_CCZ.dll`을 release 매크로로 컴파일했고 `path_fast_fixture.cpp`(40,000 random / 4,096 stateful grid / 2,000 multi-waypoint case), `officer_query_fixture.cpp` ALL PASS. 미서명 개발 산출물이며 설치하지 않았다.
- `tools/audit_current_source.py`로 공개 소스 155개 파일의 1,878개 정의 후보 위치와 지문을 다시 생성했다. 전처리된 의미 분석·함수별 ABI 검증 결과가 아니다.
- 원본 게임 EXE·세이브·타 모드 DLL·게임 dump·조조전 그림 자산·로그·백업·개인 설정·빌드 바이너리는 제외했다. 기존 홈페이지·이미지·릴리스는 유지한다.

같은 작업 폴더에서 `dll-split -Build`·설치기 검사·실게임 확인을 거쳐 v15를 배포했다. 이 공개 저장소 단독으로는 편의 DLL 전체 빌드를 하지 못한다(미포함 외부 입력, [SOURCE_DEPENDENCIES](SOURCE_DEPENDENCIES.md)). 공개 소스를 곧바로 설치 가능한 릴리스로 보지 않는다.
