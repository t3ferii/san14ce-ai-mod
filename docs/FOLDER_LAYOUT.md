# 현재 프로젝트 구조

| 경로 | 역할 |
|---|---|
| README.md | GitHub와 사용자에게 보여 줄 전체 기능·설치 설명 |
| docs/ | 현재 기능·원작 함수·개발 절차. 버전별 임시 작업 안내는 두지 않음. 대체된 조사 기록은 docs/archive/ |
| src/ · tests/ | 현재 코드·회귀 검사. 미연결 후보는 함수 조사표에서 구별 |
| tools/ | 빌드·검사·좁은 조사·현재 승격 도구. 예전 release 스크립트는 역사 보관 ZIP에 보관 |
| installer/ · distribution/ · optional_tools/ | 현재 설치 GUI·관리 스크립트·선택 도구의 원본 |
| build/input_package/ | 현재 적용할 DLL·레이아웃·manifest·설치 GUI만 보관 |
| build/development/ | 재생성 가능한 개발 출력과 현재 조사·정리·게시 근거 |
| build/*.json 등 | 검사·릴리스 증거. 기존 도구가 읽는 경로 유지 |
| development/ | CURRENT·기능 레지스트리·함수 색인·참고자료 색인 |
| assets/ | 모드가 실행 중 읽는 그림 자산(현재 `assets/ccz/` 조조전 부대 형상). 만드는 도구와 출처는 각 폴더 README |
| research/ · data/ | 미래 기능에 쓸 원작 조사·주소·ABI·원시 로그·UI/추적 자료 |
| backup/ | 현재 설치 복구에 필요한 원래 경로와 현재 정리 전 백업. 나머지는 역사 ZIP·색인 |
| legacy/ | 현재 검사/빌드가 읽는 원작 캡처와 이전 코드. 현재 모드 동작으로 취급하지 않음 |
| dist/ | 모든 이전 배포판 보존. 새 파일만 추가하며 기존 파일은 변경하지 않음 |

중첩된 `AI_TacticsMod/AI_TacticsMod/` 압축 해제본과 과거 후보 패키지는 작업 트리에서 치웠다. 옛 백업·문서·도구·작업본은 내용별 SHA256을 검사한 보관 ZIP에 모았다. `backup/ARCHIVE_INDEX.json`의 path가 보관 전 프로젝트 상대 경로다. 과거 증거에 적힌 경로를 현재 경로로 일괄 고치지 않으며, 조사 재현이 필요하면 해당 항목만 원래 경로로 꺼낸다.

원본 게임 EXE·세이브·참고 모드 폴더는 정리 대상에서 제외한다. 게임 루트의 불필요한 `SAN14_전법설명.txt`는 정리 전 백업 후 제거한다. 구버전 사용자의 수정된 TXT를 새 설치기가 임의 삭제하지는 않는다.

새 배포는 `tools/build_share.py`가 명시한 파일만 사용한다. 작업 폴더 전체를 ZIP으로 만들지 않는다.

추가 보관: `workspace_records_20261020.zip`은 현재 코드가 직접 참조하지 않는 이전 검사 기록, `workspace_loose_20261020.zip`은 구형 목표퇴각 설정·독립 무장검색 실행 파일·이전 자동 승격 CMD다. 각각 RECORDS_INDEX/LOOSE_INDEX와 ZIP 안의 ARCHIVE_INDEX로 찾는다.
