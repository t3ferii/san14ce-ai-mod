# 소스 공개 검증 (2026-10-26)

- 핵심 소스의 AI-CONTRACT 주석 20개를 추가했다. 원본과 공개본의 C/C++ 파일에서 문자열/문자 리터럴을 보존하고 주석·공백만 제외한 토큰 열이 동일함을 검사했다. 게임 로직의 실행 토큰을 바꾸지 않았다.
- 공개 폴더에서 `tools/build_public_smoke.ps1`을 실행해 종료 코드 0을 확인했다. x64 로더 dinput8.dll 컴파일 통과, `path_fast_fixture.cpp`의 40,000개 random case / 4,096개 stateful grid case / 2,000개 multi-waypoint case 통과, `officer_query_fixture.cpp` ALL PASS. 경로 fixture가 출력한 전체 assertion 실행 횟수는 394,130,919이며 독립 테스트 사례 수가 아니다.
- `tools/build_public_smoke.ps1 -Combat`도 종료 코드 0으로 통과했다. 외부 게임/payload 입력 없이 공개 소스로 실제 `SAN14_CombatAI.dll`을 release 매크로로 컴파일했다. 미서명 개발 산출물이며 설치하지 않았다.
- `tools/audit_current_source.py`로 공개 소스 140개 파일의 1,470개 정의 후보 위치와 지문을 다시 생성했다. 전처리된 의미 분석·함수별 ABI 검증 결과가 아니다.
- Python 문법, 공개 파일 허용 목록·자격 증명 패턴·개인 PC 경로, 공개본/원본 소스 토큰과 파일 지문을 검사한다. 상세 지문은 `development/public_source_manifest.json`에 기록한다.
- 원본 게임 EXE·세이브·타 모드 DLL·게임 dump·로그·백업·개인 설정·빌드 바이너리는 이번 소스 업로드 대상에서 제외했다. 기존 홈페이지·이미지·릴리스는 유지한다.

두 DLL 전체 split 재빌드, `dll-split -Build`, 원작 ABI·바이트 재검증, 신규 설치와 실게임 검증은 이번 주석/소스 공개 작업에서 실행하지 않았다. 편의 DLL을 포함하는 전체 빌드는 미포함 외부 입력을 요구한다. 공개 소스를 곧바로 설치 가능한 릴리스로 보거나 기존 DLL과 동일한 것으로 간주하지 않는다. 설치된 게임 파일과 기존 배포 ZIP은 변경하지 않았다.
