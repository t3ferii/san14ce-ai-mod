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

## 120프레임 시험 (2026-11-16, local205~206, 기준 빌드 1a4a447d)
사용자 목표는 프레임 상한 기능이 아니라, 노트북에서 상한을 풀었을 때 120프레임이 부담 없이 도는지다. 원작 사실은 `San14Sim/docs/orig/ui_native.md` 5절에 있다. 요점:
- FPS_SETTING 0/1 → 표 `1A8D5F8` {30, 60}, {1/30, 1/60}초. 게임 함수 약 35곳이 이 값을 읽는다. 날짜 진행 `44DEE0`이 시간을 프레임 수로 센다는 것은 확정(`rules_time.md`), 나머지의 쓰임은 추정이다.
- Present SyncInterval은 장치 `+0x305C`(기본 1). 타이머 대기 제한기는 정적 검색에서 못 찾았지만, **실측으로 있다**(2026-11-16 local207 ②): 수직동기를 꺼도(Present 0.15ms) 초당 60.0장, 프레임 간격 p50 16.6ms에 묶였고, 그리기 스레드는 프레임마다 CPU 3.5ms를 쓰고 약 13ms를 CPU 없이 기다렸다. 위치는 미확인이지만 표 `1A8D5F8`을 따른다: ③(`present_uncap=1`·`fps_120=1`, 원작 최고속도, 약 2분 40초)에서 게임 중 105~118fps, 프레임 간격 p50 8.0~8.9ms, p95 11~15ms, p99 15~40ms, 그리기 스레드 CPU 2.7~5.5ms, 대기 4~6ms(제한기가 1/120에 맞춰 쉼). 노트북(RTX 3070 Laptop)에서 120에 거의 닿는다. 화면이 60Hz라 60장 넘는 몫은 보이지 않는다(165Hz 모드 필요). 실행기(`SAN14_Launcher.exe`)는 FPS_SETTING 30/60만 저장하고 제한은 게임 EXE가 표로 건다. 위 "미확인" 항목의 답: FPS_SETTING은 SyncInterval이 아니라 이 표로 간다.
- 그래서 수직동기만 끄면 게임 시간이 빨라질 가능성이 크고, 바른 속도의 120프레임은 표를 120으로 바꾸고 실제로 초당 120장을 내야 한다. 실제 프레임이 120보다 낮으면 그만큼 느리게 움직인다.
- 노트북 화면(2026-11-16 조회): 2560×1600에서 60Hz와 165Hz만 있고 지금은 60Hz다. 120Hz 모드는 목록에 없다. 165Hz에 수직동기를 켜면 화면이 초당 165장을 요구하므로, 표 120과 맞추려면 120Hz 사용자 모드나 120 박자 맞추기가 따로 필요하다(측정 뒤 결정).

시험 스위치(편의 DLL `gpu_profile.cpp`, `[diagnostics]`, 모두 기본 0 = 원작, 재시작 필요). local206부터 DXGI 슬롯(팩토리, Present)을 직접 고치지 않는다. 프레임 경계는 Present 훅 소유자인 Layout의 오버레이 구독(그리기 스레드, 진짜 Present 바로 앞)이고, 게임 스왑체인(장치 `+0x3080`)이 생긴 뒤 첫 레이아웃 생성 때 구독한다:
- `frame_profile=1`: `data/diagnostics/frame_pacing.csv`, 5초 한 줄, 판마다 `#` 줄과 열 이름 줄을 다시 쓴다(local205 줄은 열이 다르다). 열: `fps`, 프레임 간격 `p50/p95/p99/max_ms`, `present_ms`(우리 구독 → 게임 `956F20` 끝, 게임 시계 `+0x32F0`; 우리 뒤 Layout 구독 그리기 + Present 안 = 수직동기·GPU 대기), `wrapper_ms`(게임이 직접 잰 `956F20` 한 번의 시간 `+0x32E0`, Present 앞 준비 포함), `out_ms`(`956F20` 끝 → 다음 구독), `cpu_frame_ms`(그리기 스레드 한 프레임 자기 CPU 전체, 스레드 사이클을 TSC로 환산), `idle_out_ms`(= `out_ms − cpu_frame_ms`, Present 밖 CPU를 안 쓴 시간의 하한), `cpu_thread_ms`(GetThreadTimes 대조값), `updates_per_s`/`renders_per_s`(앱 객체 +E8/+178), `imm_draws`/`other_draws`/`exec_lists`(컨텍스트 훅은 `gpu_profile=1`일 때만, 아니면 -1), `game_sync`/`used_sync`/`sync_changes`, `timer_ms`, `foreign_presents`(다른 스왑체인), `no_end`(게임 시계가 QPC가 아니거나 순서가 어긋나 뺀 프레임). 주 스왑체인이 아닌 Present는 첫 번째와, 주 스왑체인 프레임 없이 600번째에 로그(`gpu_profile_foreign_chain`, `gpu_profile_no_main_chain`)와 `#` 줄을 남긴다. `#` 줄: 장치 주소, 스왑체인 설명, 화면 Hz, 타이머 해상도.
- `present_uncap=1`: 게임이 Present에 넘기는 장치 `+0x305C`(SyncInterval)를 매 프레임 0으로 둔다(장치 리셋 `9571D0`이 다시 쓰기 때문). 0~4 밖 값이면 장치 구조가 다르다고 보고 거부(`present_uncap_refused`). 다음 Present부터 적용된다.
- `fps_120=1`: 표의 설정 1을 120, 1/120초로 바꾼다. 실제 값이 원작 비트(30, 60, 0x3D088889, 0x3C888888)와 다르면 거부하고 `fps120_refused`를 남긴다. `fps120_patched army=앱 객체 존재 value=프로세스 시작 후 ms`. army 0이면 슬롯 6(`154D50`)이 아직 돌지 않았다는 뜻이고, 1은 아무것도 증명하지 않는다(앱 객체는 생성자에서 정해진다). 표를 켰는데 무언가 두 배 속도로 움직이면 엔진이 1/60을 먼저 받은 것부터 의심한다.
- 읽는 법: 수직동기가 켜져 있으면 `present_ms`는 대부분 수직동기 대기이고, `present_uncap=1`일 때만 GPU 대기(와 드라이버 CPU)로 읽는다. `cpu_frame_ms`가 프레임 간격에 가까우면 그리기 스레드가 한계, `idle_out_ms`가 크면 논리 스레드나 원작 작업 대기(`B3E0D0`의 Sleep(1) 폴링). `updates_per_s`가 `renders_per_s`를 따라가면 갱신이 프레임에 묶여 있다.
- local205 시작 시 튕김(2026-11-16 두 판, 0xC00000FD 스택 넘침): 덤프의 그리기 스레드 스택에 `gpu_profile::present`와 Steam 오버레이(`gameoverlayrenderer64.dll`)가 4936번 번갈아 있었고, 맨 아래는 게임 `956F20`의 Present 호출이었다. Layout 탐침이 두 번째 스왑체인을 만든 직후부터다. 같은 서명의 첫 사고는 2026-10-31 조조전 그림 local1(`CCZ_SPRITES.md` 15줄, 게임 스왑체인 전에 건 Present 훅). 덤프에는 코드 메모리가 없지만 두 사고가 같은 방식을 가리킨다: Steam 오버레이가 그때 Present 슬롯에 있던 함수(우리 것)를 자기 원본으로 잡았고, 우리 것은 여전히 Steam이 건 DXGI Present를 불렀다. 같은 직접 패치 때문에 Layout은 자기 Present 훅을 거부했다(`layout_refused … 3`, 다른 우리 DLL). local206에서 직접 패치를 없앴다(교훈 `MOD_DEV_LESSONS.md` G17).
- 지난 `gpu_passes.csv`(11-02)의 그리기 수 0은 원인 미확인이다. `exec_lists`가 0이 아니면 지연 컨텍스트를 쓰는 것이고, 패스 시간은 그 뒤에 `gpu_profile=1`로 따로 본다.

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
