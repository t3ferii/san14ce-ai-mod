# 최종 줌아웃 렉: 원작 호출·CPU 동시 계측 (2026-11-02)

최우선 대상은 전략 월드맵에서 전체 중국 지도로 전환되기 직전의 지연이다.
추가 A/B/C 반복 대신 terrain_edit의 정상 ON 조합에서 한 번의 줌 동작을 기록한다.

## 구현

- 기존 원작 함수·COM 호출을 같은 호출 스레드에서 계측한다. 새 원작 훅·새 실행 파일·새 로그 파일은 추가하지 않는다.
- foam: 생성 E17CA0, 준비 E1B580, 파괴 E18280, 갱신 E1B7B0, 렌더 E19690, 위치/회전/스케일 setter E1AAE0/E1ADD0/E1B160 및 모드의 observe/plan/apply.
- renderer: CreateTexture2D의 원래 D3D11 구현, 모드 캡처 처리, on_device/on_present, UpdateSubresource, 진단 readback의 CopySubresourceRegion/Map, 높이 캡처 A07B70·CPU 높이·물 마스크 fitting.
- 나무: 원작 BB10F0/B91330과 모드 apply. 공통 terrain sync/check_world, 모드 Present 처리와 원래 DXGI Present도 계측한다.
- 함수별 호출 횟수, inclusive/self 경과 시간, 현재 스레드의 kernel+user CPU 시간, CPU 사이클을 집계한다. self는 계측한 동일 스레드 자식 호출을 뺀 값이다. 모든 inclusive를 합산하면 중복되므로 합산하지 않는다.
- 각 프레임에서 프로세스 전체 CPU와 Present 스레드 CPU도 함께 읽는다. CPU 시간은 실행에 소비한 시간이며, GPU 실행 시간이나 샘플링 콜스택은 아니다. CPU가 작고 경과 시간만 크면 GPU/드라이버·락·스케줄링 등의 대기 후보를 구별할 수 있다.
- 호출마다 파일을 쓰지 않는다. 메모리 집계 후 75ms 이상 Present 간격 또는 1초 주기에 기존 SAN14_AITactics.log로 출력한다. 가장 느린 호출은 그 호출 자체의 CPU와 묶고, 시작/완료 프레임 및 스레드도 남긴다.
- 스코프가 프레임 경계를 넘으면 완료 창에 집계하며 range/peak_thread가 시작 프레임을 보존한다. 빠른 함수의 CPU 시간이 0으로 양자화될 수 있어 사이클과 CPU API 유효 샘플 수도 함께 기록한다.
- 계측 API와 로그 출력에도 비용이 있으므로 결과는 진단판 측정값이다. NO_RUNTIME_LOG 빌드에서는 계측·로그가 컴파일에서 빠지고 포워딩만 남는다.

## 실행 조건

새 설정은 없으며 기존 기능설정.ini를 사용한다. 게임 종료 후 진단판에서:

```ini
[real_estate]
terrain_edit=1
[diagnostics]
enabled=1
cost_profile=0
terrain_diag_mode=0
```

groups.diagnostics/convenience도 1이어야 한다. ccz_far_keep=0 및 기존 CCZ/local14 렌더링은 유지한다.
이 지형 계측판은 terrain_edit와 진단 로그 ON으로 동작한다. 기존 cost_profile은 0으로 두어 광범위한 AI/CSV 계측을 추가로 켤 필요가 없다.
모드 0은 foam과 renderer를 모두 켠 원래 재현 조건이다. 이 작업에서는 지형을 편집할 필요가 없다.

1. 게임을 시작해 같은 세이브를 로드하고 잠깐 대기한다.
2. 전략 월드맵에서 전체 중국 지도로 줌아웃해 지연을 한 번 재현하고 다시 줌인한다.
3. 줌인 후 2초 정도 대기한 뒤 로그를 보존한다. 게임 루트에서 다음 분석 명령을 실행할 수 있다.

```powershell
python -B -X utf8 AI_TacticsMod/tools/analyze_terrain_profile.py
```

가장 큰 Present 간격 3개와 해당 집계 창의 함수별 비용을 표시한다. --frames 1 --rows 20 또는 --json도 지원한다.
로그를 전달하면 CPU 연산 집중, 모드 자체 비용, 원작/드라이버 대기 중 어디에 시간이 쓰였는지 비교할 수 있다.
측정 전에는 어느 호출이 렉 원인인지 확정하지 않는다.

## 주요 로그 필드

| 이벤트 | army | value | hex |
|---|---|---|---|
| terrain_profile_on | 논리 CPU 수 | 느린 프레임 기준 us | 주기 us |
| terrain_frame | 완료 프레임 | 직전 Present 간격 us | 집계 창 us |
| terrain_frame_cpu | 완료 프레임 | 프레임 중 전체 프로세스 CPU us | Present 스레드 CPU us |
| terrain_cpu_window | 완료 프레임 | 집계 창 전체 프로세스 CPU us | 집계 창 us |
| terrain_wall_함수 | 보고 프레임 | 누적 경과 us | 최대 호출 경과 us |
| terrain_cpu_함수 | 보고 프레임 | 누적 스레드 CPU us | 최대 호출 CPU us |
| terrain_self_함수 | 보고 프레임 | 자체 경과 us | 자체 CPU us |
| terrain_calls_함수 | 보고 프레임 | 호출 수 | 스레드 ID(복수이면 FFFFFFFF) |
| terrain_cycles_함수 | 보고 프레임 | 누적 천 사이클 | 최대 천 사이클 |
| terrain_peak_함수 | 가장 느린 호출의 완료 프레임 | 해당 호출 경과 us | 해당 호출 CPU us |
| terrain_peak_thread_함수 | 가장 느린 호출의 완료 프레임 | 해당 스레드 ID | 해당 호출 시작 프레임 |
| terrain_range_함수 | 보고 프레임 | 첫 시작 프레임 | 마지막 완료 프레임 |
| terrain_valid_함수 | 보고 프레임 | CPU 유효 샘플 수 | 사이클 유효 샘플 수 |

Present 스레드가 바뀌거나 첫 기록이라 CPU 차이를 계산할 수 없으면 terrain_frame_cpu.hex=FFFFFFFF이다.
일반 함수의 CPU 최대값과 wall 최대값은 서로 다른 호출일 수 있다. 동일 호출 비교는 terrain_peak를 사용한다.

## 검사

- terrain-profile 경로: 31개 C++ 진단/배포 fixture 및 2개 로그 파서 검사 통과. 중첩 self/CPU, 다중 스레드, 프레임 경계, 예외, 8인자/64비트 반환, CPU API 실패, OFF 무계측, 배포 문자열 제외를 확인했다.
- foam: 진단/배포 각각 100951개, 나무: 7143/7087개 및 원작 정적 29개 통과.
- renderer: 기존 7종 fixture(r2/r2b/cliff/water_mask/site/early/base)의 진단/배포 컴파일·실행 통과. D3D11 WARP readback 포함. 기존 전체 소스 preimage 검사와는 별도 검사다.
- check_feature.ps1 -Feature dll-split -Build 통과. 이후 peak CPU 메타데이터를 보완한 헤더는 단독 fixture 및 최종 진단 DLL 빌드로 재검증해 통과했다.
- 위 검사는 실게임 줌 전환 관찰이 아니다.

## 최종 로컬 적용

- v15.1-terraincpu-local1: installed_and_verified, 미서명 진단판. Convenience만 교체.
- SAN14_Convenience.dll SHA-256: 65cdd04c74055be7cf0e03fa306b02e19d3e76627059546b76877f72934ec294.
- 설치본/Apply/매니페스트 해시 및 소스 스냅샷 일치. CCZ/CombatAI/dinput8은 설치 전 해시 그대로 유지.
- 설치 당시 기존 설정을 모두 보존했고, 계측 활성화를 위해 이후 diagnostics.enabled만 0→1로 변경했다. terrain_edit=1, terrain_diag_mode=0, cost_profile=0, ccz_far_keep=0은 유지했다.
- 백업: backup/local_install_3141390e8a404eb6adc4d2cddae60599.
- 설치 증명: build/v15_1-terraincpu-local1_installation.json.
- 검사 기록: build/development/terrain_profile_diagnostic_build.log, terrain_profile_split_check.log, terrain_profile_foam_check.log, terrain_profile_trees_check.log.
- 게임 종료 후 이전 로컬판 복구: `python -B -X utf8 AI_TacticsMod/tools/install_local.py rollback v15.1-terraincpu-local1`. 이 명령은 현재 사용자 설정을 보존한다. 일반 진단 기록도 끄려면 설치관리자에서 진단 로그를 OFF로 저장한다.
- 실제 게임의 최종 줌아웃 구간 계측 결과는 사용자 실행 후 확인한다. 공개 배포 작업 없음.
