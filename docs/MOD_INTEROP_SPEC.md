# 우리 모드 내부 호환 규칙 (훅·체인·서비스)

사용자 지시 2026-11-16 「1+2단계는 우리 모드 자체 리팩토링」. 이 문서는 우리 모드 안의 규칙만 다룬다. 다른 모드(통합패치·발탁무장 등)와의 호환 자료는 아직 없다. 타모드 자체는 현재 버전을 갖고 있다(`reference_mod/san14ce-integrated-patch` source_R43.38, `reference_mod/SAN14CE_ModPack_2.4_sam123123` 발탁 1.9). 버린 것은 우리가 예전에 옛 설계 문서(LAYOUT_DLL_COMPAT_DESIGN: 발탁 1.8, 안개 1.7, 육아 1.1, R1 모드팩, 옛 게임 빌드 주소)에서 옮겨 적었던 호환 표뿐이다. `development/foreign_hooks.json`은 지금 비어 있는 schema 2이고, 나중에 위 현재 버전 소스로만 다시 채운다. 4계절 패치는 우리가 작업 중이라 대상이 아니다.

## 1. 훅 정책 (매니페스트 어휘)

모든 훅 자리는 `tools/hook_map.py`가 찾고, `tools/gen_module_manifests.py`가 모듈 매니페스트의 `hooks[].policy`로 적는다.

| 정책 | 뜻 | 우리 쪽 예 |
|---|---|---|
| chain | 그 자리에 있던 값(다른 훅 또는 원작)을 받아 두었다가 부른다 | Layout `CHAIN_OK` 행, 출진 E8 연결, wndproc, COM |
| yield | 같은 함수에 다른 훅이 있으면 비켜선다 | Layout `YIELD_IF_PEER`(CHAIN_OK 없음) |
| refuse | 낯선 값이 있으면 설치하지 않는다 | Layout `REFUSE_IF_FOREIGN` |
| refuse_if_changed | 원래 바이트·슬롯 값이 그대로일 때만 직접 패치 | 가드 해시 검사 자리 |
| exclusive | 체인·양보·거절 표시가 없는 Layout 행 | 단독 소유 자리 |

새 훅을 추가할 때:
1. vtable 슬롯은 `layout_registry.h`에 행을 두고 `ROUTE_VT(slot, original, repl, &next)`로 건다. 대체 함수는 원작 주소(`GA(...)`)가 아니라 `next`를 부른다.
2. 설치 전이거나 `next`가 아직 원작이면 `fn<T>(rva)`로 부른다(게임에서는 같은 주소, 모의 검사에서는 목 함수). 예: `native_advisor_briefing.inl`의 `slot_ptr`.
3. 여러 슬롯을 한 번에 걸면 `layout_client::current_mark()` → 실패 시 `rollback_to(mark)`로 전부/전무.
4. `development/hook_sites.json`의 바인딩 call 이름을 실제 등록 함수(`ROUTE_VT` 등)로 맞춘다. 맞지 않으면 hook_map R9("행에 등록 코드 없음")가 실패한다.

## 2. 진입점(UI) / 기능 / 엔진 서비스 분리

- 서비스 목록은 `development/module_layers.json`이다. 모듈 매니페스트의 `provides`/`uses`가 여기서 나온다.
- 엔진 서비스의 켜짐 조건은 그 서비스를 쓰는 기능 스위치들의 OR이다. 예: 지형 엔진 `terrain_core_on()` = `terrain_edit || season_tiles` (`src/terrain_core.h`, 정의는 `native_adapter.cpp`). 렌더·거품·나무·물 진단·높이 연결은 이 조건을 쓴다.
- UI 진입점(부동산 개발 메뉴 `kPointerTerrain`, 사이드카 저장, 클릭 로그)은 자기 기능 스위치(`terrain_edit`)만 본다.
- 숨은 스위치 의존은 `tools/switch_deps.py --check`가 막는다(기준 `development/switch_deps_baseline.json`, 현재 0건). 설치기 설명의 「필요:」는 실제 코드 의존과 같아야 한다.

### 모듈 사이 연결 (2026-11-16)
- 한 모듈이 다른 모듈의 함수·스위치를 써야 하면 직접 이름으로 부르지 않는다. 허브 `native_split_install.inl`의 `split_bind_links`(split_prepare 첫머리, 어떤 검사·설치·게시보다 먼저)가 연결을 채운다: 함수는 같은 함수의 포인터, 스위치는 `mod_features` 칸의 주소(나중의 split_feature_off가 그대로 보인다). 현재 `personnel_links`(군단 내정 3함수 + 부상 스위치), `auto_commands_links`(군사 보고 카드 + 스위치).
- 두 DLL이 함께 보는 계약 헤더(`ambush_shared.h`, `season_mobility_shared.h`, `computer_tactics.h`)는 core 소유 서비스 파일이다. 다른 모듈 함수 선언이 필요하면 그 모듈이 소유한 헤더(`ambush_hook.h`)를 포함한다.
- 같은 슬롯을 번갈아 게시하는 협력(부상↔군단 내정: 부상이 켜지면 부상이, 아니면 군단 내정이 134C580·1355B18 게시)은 HOOK 기준선에 사유와 함께 둔다.

## 3. 검사

| 무엇 | 명령 |
|---|---|
| 훅 지도 | `python tools/run_checks.py --routes hook-map` |
| Layout 행 수·소유 | `--routes layout-registry,split-module` |
| 매니페스트·호환 행렬·스위치 의존 | `check_feature.ps1 -Feature module-manifests` |
| 타모드 행렬(자료 생기면) | `python tools/mod_compat.py --foreign all` |

## 4. 원칙

- 우리 모드만 설치했을 때 동작은 바뀌지 않는다: 체인의 다음은 원작이고, 엔진 서비스 OR은 예전 조건을 포함한다.
- 스위치 끔·DLL 없음 = 원작 또는 이전 모드 동작.
- 원작 사실은 `San14Sim/docs/orig`에만, 이 규칙과 구현 기록은 `AI_TacticsMod/docs`에만 둔다.
