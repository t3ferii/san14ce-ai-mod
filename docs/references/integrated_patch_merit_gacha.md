# 통합패치(wprkfdid12-ux) 13번 논공 개성 가챠와 우리 모드 호환 (2026-11-14, Fable 소스 대조)

출처: `reference_mod/san14ce-integrated-patch/source_R43.38/mt/src/`(저쪽 = T), 우리 `src/`(O). 소스 대조만, 실게임 미확인.

## 결론
- 빌드 같음: 저쪽 리졸버 타임스탬프 `0x6ad06f53`·이미지 `0x2a02000`(T/merit_resolver.c) = 우리 주소표(O/game_addresses_data.inl).
- 가챠 적재: 로더 → `SAN14_ModCore.dll` → `SAN14Mods\MeritTraits\SAN14_MeritTraits.dll` → `SAN14_MeritResolver_CEKO.dll`. 훅 39개, 논공 확정은 `1ED8F0` 호출 지점(플레이어 `6CE41C`, 컴퓨터 `2D6D6`). 하나라도 원래 바이트가 아니면 **전체 설치 거부**(T/merit_runtime.c:442-452).
- **우리 논공 켜짐 + 우리가 먼저 설치 → 가챠 꺼짐**: `1ED8F0` 입구 6바이트(E9+NOP, O/native_merit_links.inl:6,74)를 저쪽이 거부, `2ABDE0`은 함수 해시 불일치.
- **우리 논공 끔**: 위 입구 훅은 안 걸림(O/native_merit_links.inl:62,87,134,137). 남는 두 가지:
  1. 저장 연결 `3FA342`는 늘 게시(명품창고 등 저장 부속 7개 공용). 우리가 먼저면 저쪽 R40이 우리 모양을 알아보고 체인, 저쪽이 먼저면 우리 저장 연결·논공·수여가 꺼지고 가챠는 정상.
  2. 군사 보고(`game_flow.advisor_briefing`, 기본 꺼짐)가 켜져 있으면 `1F1DA0` 입구를 덮어 가챠 거부.
- traits9 는 호환(5자리 값 같음, `Traits9Check.ini` 있으면 우리가 화면 양보, O/native_layout_core.inl:139).

## 충돌 표
| 주소 | 저쪽 | 우리 | 우리 플래그 | 결과 |
|---|---|---|---|---|
| 1ED8F0 | 앞 16바이트 검사 + 호출 지점 훅 | 입구 6바이트 | personnel.merit | 가챠 거부 |
| 2ABDE0 | 0x176바이트 해시 | 입구 5바이트 | personnel.merit | 가챠 거부 |
| 6DBAA0 | 앞 16바이트 검사(점프 허용) | 입구 5바이트 | personnel.merit | 통과 |
| 3FA342 | 저장 쓰기 훅 7바이트 | E8+NOP 2 | 없음(항상) | 순서에 따라 체인 / 우리 저장 꺼짐 |
| 1F1DA0 | 앞 16바이트 검사(예외 없음) | 입구 점프 | game_flow.advisor_briefing | 가챠 거부 |
| 4A90B3 등 5자리 | 5→9 | 같은 값 | extensions.traits9 | 호환 |

## 우리 쪽 최소 변경안 (저쪽 수정 없음)
1. 감지: `native_layout_core.inl` `peer_rules`에 `SAN14Mods\modules\MeritTraits.ini [Mod] Enabled` 조건(파일 기준, 설치 순서 무관; R43 설치본에서 이름 확인 필요).
2. 감지되면 `f.merit=false` + 로그(입구를 5바이트로 줄여 체인은 `2ABDE0` 해시 때문에 불가).
3. 감지되면 군사 보고의 `1F1DA0` 입구 훅 양보(해설 대사 한 종류만 빠짐).
4. `3FA342` 역방향 체인: 이미 `E8 rel32 90 90`이고 대상이 EXE 밖이면 그 뒤로 이어 붙임(저쪽이 먼저여도 우리 저장 부속 유지).
2~4는 공용 파일(`native_adapter.cpp`, `native_split_install.inl`) 수정.

## 불확실
- 설치(로드) 순서 경합 미검증(docs/LAYOUT_DLL_COMPAT_DESIGN.md:48,641).
- 4번 체인 시 우리 브리지 끝의 `cmp [rbx+18h],10h`(O/native_merit_links.inl:50) — 저쪽이 C 함수 안에서 부르면 RBX 보장 없음.
- 저쪽 STATUS_R43.md:362-369: 저쪽 "AI 호환판"이 우리 논공·명품 저장 연결을 스스로 끄게 바꿨다고 기록.
- 두 논공 동시 켜짐 시 보상 섞임, 저쪽 `SAN14_Traits9_UI.dll`(소스 없음) 미대조, 우리 설치기가 개성 화면을 9칸판으로 바꾼다는 저쪽 기록(notes_a10.md:18) 미확인.
