# GPU 렌더링 부하 조사 (2026-10-19)

정적 조사만 했다. 게임 실행 중 GPU 사용률·FPS 측정, 수정, 설치는 하지 않았다.

## 대상
- 분석 파일: `build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe` (sha256 `1fbe6be7…`). 원본은 현재 `SAN14CE_KO.exe` `87ed4e46…`과 같다.
- 시스템: RTX 3070 Laptop GPU, 바탕화면 2560×1600 60Hz.
- 레지스트리 `HKCU\Software\KoeiTecmo\SAN14\Configs`: WIDTH 1920, HEIGHT 1200, WINDOW_MODE 1, CG_QUALITY 1, FPS_SETTING 1. 런처 선택지 문자열은 30FPS와 60FPS.

## 원작 코드 근거
- `1403F0A20`: Configs 레지스트리를 읽고 쓴다. FPS_SETTING은 설정 객체 `+0x77C`, CG_QUALITY는 `+0x75C`, WINDOW_MODE는 `+0x6F8`에 저장된다.
- `140BFC820` → `140954730`: 프레임마다 Present를 호출한다. `IDXGISwapChain::Present`(vtable `+0x40`)의 SyncInterval은 장치 `+0x305C`, Flags는 0이다. 보조 swapchain 경로는 SyncInterval 0을 쓴다.
- `140948CDB`: 장치 기본 SyncInterval은 1이다. `140CAA913`/`140CA6ACD`는 설정 블록 `+0x120` ↔ 장치 `+0x305C`를 복사한다.
- 미확인: FPS_SETTING(+0x77C)이 SyncInterval이나 프레임 타이머로 이어지는 경로. 따라서 30FPS 설정이 GPU 부하를 실제로 절반으로 줄이는지도 확인하지 않았다.

## 판단
- 원작은 수직동기 Present(1)를 쓰므로 60Hz 화면에서는 이론상 60FPS 이하다. 다만 드라이버에서 수직동기를 강제로 끄면 이 제한이 사라진다.
- 과열의 유력한 원인은 두 가지다. 하나는 정지 화면, 메뉴, 백그라운드 창에서도 60FPS로 전체 장면을 계속 그리는 것이다. 다른 하나는 그림자·SSAO·반사 그림자 맵 같은 장면 비용이다.
- 모드 후보: Present 직전에 프레임 제한을 둔다. 게임 함수는 호출하지 않는다. 창이 비활성이거나 최소화되면 낮은 FPS로, 활성 상태에서는 선택한 상한으로 제한한다. 소유자는 편의 DLL이다. 원작 렌더 스레드에서 대기만 하고 게임 메모리 구조는 바꾸지 않는다.

## 실측 1차 (2026-10-19 00:35~00:37, `data/diagnostics/gpu_states_20261019.csv`)
- 게임이 00:37:01에 종료되어 정지·진행·메뉴·최소화 단계는 측정하지 못했다. 아래 값은 준비 구간 90초(로드와 지도 화면 추정)이다.
- 게임이 앞 창일 때(50초): GPU 사용률 약 40~60%, 전력 약 95~107W, 클럭 약 1850~1900MHz(P0), 온도는 67°C에서 86°C까지 올랐다.
- 게임이 뒤 창일 때(최소화 아님, 40초): 사용률 약 40%, 전력 약 58~95W. 백그라운드에서도 렌더링 부하가 거의 줄지 않는다.
- 게임 종료 후: 약 18W(P8), 사용률 0~2%.
- 해석: 사용률은 40%대인데 전력과 클럭은 최대 부스트에 가깝다. 60FPS 상한 안에서도 GPU가 고클럭을 유지한다. 비활성 창 FPS 제한이 효과를 낼 여지가 크다(측정이 아니라 추정).

## 실측 4차 — 상태별 (2026-10-19, `data/diagnostics/gpu_states_20261019_run4.csv`)
각 단계 45초, 1초 간격으로 측정했다. 전환 직후 5초는 평균에서 뺐다. 2·3차는 중단한 시도다.

| 상태 | 사용률 평균 | 전력 평균(최대) | 클럭 | 온도 |
|---|---|---|---|---|
| 전략 지도 정지 | 40% | 78W (80) | 1632MHz | 81~83°C |
| 전략 지도 진행 | 44% | 88W (104) | 1718MHz | 83~87°C |
| 메뉴(정지) | 47% | 79W (86) | 1523MHz | 86~88°C |
| 백그라운드(창 보임) | 48% | 75W (79) | 1434MHz | 86~87°C |
| 최소화(41/45초 확인) | 48% | 73W (74) | 1402MHz | 86~87°C |
| 참고: 게임 종료 | 0~2% | 약 18W | 210MHz | — |

- 정지 화면, 메뉴, 백그라운드, 최소화 상태에서도 GPU 작업량이 거의 일정하다. 원작은 화면이 바뀌지 않거나 보이지 않아도 매 프레임 전체 장면을 그린다.
- 진행 중에만 전력이 약 10W 더 높다. 기본 부하(약 75W)의 대부분은 상태와 무관하게 고정된 렌더링 비용이다.
- 결론: 효과가 가장 큰 수단은 상태별 프레임 상한이다. 최소화나 백그라운드에서는 화면을 거의 그릴 필요가 없다. 정지·메뉴 화면도 낮은 FPS로 충분할 가능성이 있다(체감은 미확인).

## 단계별 GPU 계측 구현 (2026-10-19, 미설치)
- `src/gpu_profile.cpp`/`.h`: 편의 DLL 전용. `기능설정.ini [diagnostics] gpu_profile=1`일 때만 동작하고 기본값은 0이다. 게임 호출은 그대로 넘기는 관찰 전용이다.
- 시작 시 임시 DXGI 팩토리에서 CreateSwapChain/ForHwnd vtable 슬롯을 교체한다. 게임 스왑체인의 Present를 걸고, 첫 Present에서 그 장치의 즉시 컨텍스트에 OMSetRenderTargets(+UAV), Draw 7종, Dispatch 2종을 건다.
- 렌더타깃이 바뀔 때마다 타임스탬프를 기록하고 4프레임 늦게 DONOTFLUSH로 읽는다. 5초마다 `data/diagnostics/gpu_passes.csv`에 단계별 평균·최대 ms, 비중, Draw 수를 기록한다. 줄 머리 `#`는 이벤트 줄이다.
- 연결 3곳: `src/native_adapter.cpp`의 `#include "gpu_profile.h"`, `src/native_split_install.inl`의 `gpu_profile_start(self_module)`, `tools/build_split.ps1`의 `gpu_profile.cpp`와 `dxguid.lib`.
- 검사: `check_feature.ps1 -Feature dll-split -Build` 통과(개발 빌드만, 설치·ZIP 변경 없음). 실게임 동작은 미확인이다. 게임 스왑체인이 설치 전에 만들어지면 Present를 놓친다. 이 경우 CSV에 `installed` 줄만 남는다.
- 보류 사유: 같은 시점에 다른 세션이 공통 소스에 지역 무장 기능을 추가하고 있었다. 그 작업이 끝나면 연결 3곳을 재확인하고 다시 빌드·검사한 뒤 설치한다.

## 다음 확인
1. 실게임에서 전략 지도 정지·진행·메뉴·백그라운드 상태별로 GPU 사용률과 전력을 측정한다(`nvidia-smi --query-gpu=utilization.gpu,power.draw,temperature.gpu --format=csv -l 1`).
2. 런처를 30FPS로 바꿔 같은 측정을 반복한다.
3. NVIDIA 제어판에서 이 게임의 수직동기·최대 프레임 속도가 강제되어 있는지 확인한다.
