# 최종 줌아웃: 생성 후 검증 경로 수정 (2026-11-02)

## 실게임 로그 근거

사용자 실행 뒤 보존한 로그: data/diagnostics/terrain_zoom_cpu_20261102_051635.log.
SHA-256: 783d8a2ca20ebe70fb265b8a193ac81df2fce060ff0316b946cb425b160680ee.
분석 출력: build/development/terrain_zoom_before_fix.json.

| 항목 | 측정 |
|---|---|
| 프레임 620 Present 간격 | 570.542ms |
| 같은 프레임 중 프로세스 전체 CPU | 640.625ms (복수 스레드 합계) |
| foam_create_hook | 222회, 경과 523.839ms, CPU 312.500ms |
| 위 훅의 계측 자식 제외 경과 | 523.031ms |
| 원작 E17CA0 | 경과 약 0.814ms (창 경계에서 native 223회/완료 훅 222회) |
| 훅 시작/완료 프레임 범위 | 620→620 |
| 훅 실행 스레드 | 28308 |
| DXGI Present 스레드 | 24896 |

초기 로딩 프레임 1의 20초는 이 줌 지연 판정에서 제외한다.
이 데이터는 생성 후 모드 처리에 큰 시간이 소비되었음을 직접 보여준다.
경로 안의 개별 VirtualQuery만 따로 측정한 값은 아니므로, VirtualQuery 하나가 523ms였다고 단정하지 않는다.

## 수정

원작 생성 성공 콜백이 반환한 살아 있는 객체에서, vtable과 필요한 위치/회전/크기를
SEH 예외 보호 아래 한 번 읽는다. 기존의 호출마다 전체 메모리 영역을 조회하는
VirtualQuery 루프를 제거했다. 타입 불일치·null·접근 불가 포인터는 계속 거부한다.

원작 함수의 인자·반환값·호출 횟수·주소·바이트 가드, 35개 원본 물살의 매칭 및 이동/숨김/복원
규칙은 유지한다. 다른 게임 함수를 새로 호출하거나 실행 중 메모리를 외부에서 조작하지 않는다.
CCZ 렌더링 및 지형 텍스처 편집 코드는 변경하지 않는다.

계측판의 기존 경과/CPU 로그를 유지해 수정 전후 동일 구간을 비교한다.
이 수정이 모든 줌 지연을 해결했는지는 새 실게임 기록으로 확인해야 한다.

## 검증

- terrain_foam_build.ps1: 원작 5개 슬롯·8개 함수 지문 통과, 진단/배포 각각 100962개 검사 통과.
- null 핸들/객체, 외부 vtable, 접근 불가 핸들/객체, 읽을 수 있는 vtable 뒤의 접근 불가 transform을 거부하는 검사 추가.
- 원래 위치·회전·크기의 비트 단위 보존 확인.
- check_feature.ps1 -Feature dll-split -Build 및 RuntimeDiagnostics 빌드 통과. 내보내기 함수 2개 유지.

## 최종 적용

- v15.1-terrainfix-local1, 미서명 진단판: installed_and_verified.
- 편의 DLL만 교체. CCZ/local14, 전투 DLL, 입력 로더 및 모든 사용자 설정을 설치 전 그대로 보존.
- Convenience SHA-256: 1f45279f4b099d53ca1011b51175ebcd151b18b3d0c08c26472f73dfd15ead3c.
- 설치본/Apply/매니페스트 해시와 소스 스냅샷 일치.
- 설치 증명: build/v15_1-terrainfix-local1_installation.json.
- 백업: backup/local_install_2b4a7e4a22e043b09fc90fe23a465f1b.
- 검사 기록: build/development/terrain_zoom_fix_foam_check.log, terrain_zoom_fix_split_check.log, terrain_zoom_fix_diagnostic_build.log.
- 이전 계측판으로 복구: 게임 종료 후 `python -B -X utf8 AI_TacticsMod/tools/install_local.py rollback v15.1-terrainfix-local1`.
- 수정 후 실게임 효과는 아직 확인하지 않았다. 같은 줌 동작 한 번으로 기존 계측판과 비교할 수 있다. GitHub 공개 작업 없음.
