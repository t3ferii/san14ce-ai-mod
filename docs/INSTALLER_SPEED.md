# v12 설치·원본 복구 지연 수정

v11.31 이후 설치기는 다른 화면 모드의 변경을 보존하려고 ScreenLayout.rdb의
일부 항목만 병합·복원한다. 이전 백업의 설치기는 파일 전체를 복사하는 방식이었다.
병합 자체보다 PowerShell에서 18,046개 항목을 대상 8개마다 반복 검색하고,
검색할 때마다 locator를 해석하는 구현이 지연의 주요 원인이었다.
항목마다 `New-Object byte[]`를 호출하는 비용도 있었다. EXE 해시는 약 0.1초였다.

읽을 때 KTID→위치 색인을 한 번 만들고, 변경할 항목만 해석한다.
바이트 배열은 .NET 생성자를 직접 사용한다. 중복 KTID 거부·다른 모드의 변경 보존·
기존 백업·해시 대조·실패 시 복구는 유지한다. 게임 EXE나 모드 DLL의 동작 변경은 없다.

같은 전체 레이아웃과 배포 파일을 별도 게임 폴더 두 곳에 복사해 실제 CMD로 측정했다.
입력 메뉴 대기 없이 실행한 한 차례 비교이며 PC와 캐시 상태에 따라 시간은 달라진다.

| 작업 | 이전 | 수정 후 |
|---|---:|---:|
| 설치 | 16.85초 | 5.02초 |
| 같은 버전 재적용 | 34.15초 | 3.88초 |
| 원본 복구 | 22.89초 | 3.04초 |

설치 결과 인덱스 해시가 동일하고 복구 후 원본 인덱스와 바이트 단위로 일치했다.
별도 회귀 검사는 변경된 항목·중복 KTID·없는 KTID 보존을 확인한다.

- 선택 검사: `tools/check_feature.ps1 -Feature installer` (DLL 빌드 불필요)
- 비교 근거: `build/installer_speed/cmd-timings.json`, `layout-before.json`, `layout-after.json`
- 재현: `tests/verify_installer_speed.py`는 `build/installer_speed`의 이전 스크립트·
  인덱스 스냅샷을 사용하고 새로운 `cmd_before`/`cmd_after` 폴더를 요구한다.
- 현재 설치기와 갱신 전 ZIP 백업: `build/installer_speed/publish.json`의 backup 경로.

이 성능 수정은 설치기 스크립트만 바꾼다. 설치·복구 비교는 격리 복사본에서 했다.
같은 v12 배포에 포함된 툴팁 3줄 수정은 별도의 편의 DLL 변경이다.
