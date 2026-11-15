# 게임 패치 내성 (2026-11-05 사용자 지시, 2026-11-07 W1-E 재기준 반영)

기준판은 Steam buildid 25953386, EXE `1a4a447d…`(TimeDateStamp 0x6AD06F53) 하나다. 옛 빌드 1fbe6be7은 더 이상 지원·비교 대상이 아니다(이력은 `docs/HISTORY.md`, 옛 산출물은 병합 뒤 `backup/`).

## 원칙
1. **기능 코드에 원작 RVA 숫자를 쓰지 않는다.** 모든 원작 주소는 중앙 주소표 한 곳(`src/game_addresses.h` + 생성 데이터 `src/game_addresses_data.inl`)에만 둔다. 기능 코드는 `GA(GA_<기준판 RVA>)`로 해석된 주소를 쓴다. 정적 검사 `tools/check_addresses.py (b)`가 "기능 소스에 원작 범위 16진 상수 0개"(허용 목록 제외)를 지킨다.
2. **주소는 실행 시 해석한다.** 알려진 빌드(EXE `TimeDateStamp`가 `ga_builds`에 있음)면 미리 계산된 표(열 하나), 모르는 빌드면 서명·RTTI 해석(`ga::resolve_image`, 읽기만 한다). 해석 못 한 id는 0이고, 0을 쓰는 소스 파일의 기능은 `addr_unresolved` 로그와 함께 꺼진다(`src/game_addresses_gates.inl`).
3. **해석 방법은 패치에 강한 순서로 고른다.** RTTI 클래스 이름 → vtable 슬롯, 다른 슬롯이 든 함수 포인터(slotfn), 함수 시작 서명(유일 일치만), 함수 안 패턴(mid), RIP 상대 데이터 참조(dataref), 표 값만(table, 알려진 빌드 전용).
4. **가드는 현재 빌드의 정확한 바이트다.** 소스의 기대 바이트 리터럴과 `GH(0x…ull)` FNV-1a 해시는 기준판 값이다. 알려진 빌드에서는 `ga::mcmp`/`ga::mcmp_at`이 그대로 memcmp(다른 모드가 건드린 바이트를 모두 본다), 모르는 빌드에서만 재배치 칸(rel32·RIP disp)을 `ga_wild`로 무시한다. `GH()`는 항등 표지(도구가 가드 해시 행을 알아본다).
5. **검사로 고정한다.** `tools/check_addresses.py`(dll-split `address-table`): (a) 실제 해석기가 방법 있는 모든 id에서 표 값과 같은 값을 내는지, (b) 원작 범위 상수 0개와 `GH()` 감싸기, (c) 실제 모듈 가드가 해제본에서 모두 통과(CCZ 제외)·정확 바이트 변조 감지·관찰 훅 허용, (d) `addr_promote check`(GH 행 = 이미지 FNV, 가드 리터럴 = 이미지 바이트), (c4) 관찰 훅 표.
6. **설정 키를 새로 만들지 않는다.** 해석 캐시 파일도 만들지 않는다(시작 시 해석, 알려진 빌드는 표 복사).

## 표의 구조 (단일 열)
- `GaId` 열거자 `GA_<RVA>`: 이름은 기준판 RVA 16진수, 값은 조밀 색인. 같은 RVA는 한 id다. 값 0인 id는 없다(2026-11-08 W8에서 미사용 거부 id `GA_X2B09F9`(옛 컨테이너)·`GA_X2E111D`(제거된 traits9 captive 행)를 삭제하고, `GA_2B2E28`의 mid 기준을 새 컨테이너 `GA_2B2CC9`로 다시 도출했다). 쓰는 곳이 없는 0 id 14개는 W1-E에서 삭제했다(`development/addr_promote/1fbe6be7_to_1a4a447d/dropped.tsv`). 현재 id 2,056개 전부 해석된다.
- `ga_entries`(종류, 방법, 서명/RTTI 매개), `ga_users`(소스 파일 → id 비트맵, 게이트가 쓴다), `ga_wild`(모르는 빌드 전용 와일드카드, 기준판 이미지 디코드), `ga_val_1a4a447d`(값), `ga_builds`(이름, 타임스탬프, .text 크기·CRC, 이미지 크기), `ga_probes`(준비 판정용 12곳), `ga_observers`(San14Sim 관찰 훅 24곳).
- 옛 열, 정확 바이트 창(`ga_expb_*`), 해시 짝(`ga_hash_pairs`)은 없다.
- 읽고 쓰는 도구: `tools/ga_table.py`(파서·방출기). 기록 파일은 `development/addr_promote/<이전>_to_<새>/`.
- 호환 별칭은 없다(W8, 2026-11-07): W1-E가 다른 세션 소유 전법 우선순위 소스를 건드리지 않으려고 두었던 `src/game_addresses_compat.h`와 `development/addr_promote/compat_files.txt`를 삭제했고, 그 6개 소스도 `GA_<1a4a447d RVA>`를 직접 쓴다. `addr_promote`는 별칭을 만들지 않고 모든 추적 소스를 한 번에 고친다.

## 새 패치가 나왔을 때 (`tools/patch_next.py`가 읽고 보고하고, `tools/addr_promote.py`가 기록하며 고친다)
`patch_next`는 게임을 건드리지 않는다: 설치 EXE 헤더·Steam appmanifest·해제본 이미지를 읽기만 하고, 게임 실행·메모리 쓰기·설치는 없다. 산출물은 `build/development/patch_next/<대상 sha8>/`(`build.json`, `addr_table.json`, `resolve.json`, `guards.json`/`guards.tsv`, `hookmap_diff.json`, `report.md`/`report.json`, `emit/game_addresses_data.inl`, `stage/`).
1. `python -X utf8 tools/patch_next.py detect`: 설치 EXE(`SAN14CE_KO.exe`)의 TimeDateStamp·SizeOfImage·sha256과 appmanifest buildid를 표의 기준판과 비교한다(`current`/`new`). 새 빌드이고 해제본이 없으면 다음 단계를 안내한다.
2. EXE를 Steamless로 해제(`build/ui_native_analysis/editor83_b<buildid>/game.exe.unpacked.exe`)하고 Ghidra 추출본을 만든다(사용자 단계, `docs/SESSION_RULES.md` 7).
3. `python -X utf8 tools/patch_next.py run --target auto`(또는 sha8·경로). 아래 단계를 차례로 하고 `report.md`를 쓴다. 단계마다 따로도 돈다.
   - `resolve`(`gen_signatures.py analyze --ref current --target <새>`): 표의 해석 방법(sig/mid/rtti/slotfn/dataref)을 새 이미지에서 Python 거울로 돌리고, 못 찾은 id는 기준판과 새 이미지 **양쪽에서 유일한** 새 패턴을 기준판에서 도출한다(`derived`). 방법 없는 표 전용 id는 같은 주소에서 함수 본문이 같을 때만 받는다. 이어서 실제 C++ 런타임 해석기(`tests/addr_resolver_fixture.cpp`, 후보 표로 만든 스테이지 빌드)를 새 EXE에 돌려 모든 방법 id의 값이 Python 결과와 같은지 대조한다(불일치 0이어야 한다).
   - 판정: OK(같은 주소, 본문 동일), AUTO(다른 주소, 본문이 재배치·점프 테이블만 다름), ATTENTION(주변 함수가 바뀜·오프셋 이동·표 전용 id·새 패턴이 순서를 벗어남 → 사람이 `file:line`을 보고 결정), BLOCKED(못 찾음 → 그 id를 쓰는 소스의 기능은 꺼진다), DENIED(기준판에서 이미 거부).
   - `guards`: 기준판에서 실제로 실행되는 모든 `ga::mcmp`/`mcmp_at`/가드 해시(`tests/ga_equiv_dump.cpp` 덤프)를 새 이미지의 대응 위치에서 평가한다(OK / RELOC = 재배치 칸만 달라 런타임 가드가 통과 못 함 / CHANGED / UNMAPPED). 같은 후보 표로 만든 가드 픽스처(`GA_FIXTURE_BUILD=1`)를 새 EXE에 돌려 `ga_gate` 주소 게이트와 모듈 가드가 꺼 버리는 기능 목록, 곧 빌드 게이트 표(옛 `expected_disabled.json`의 후속)를 만든다.
   - `hookmap`(`hook_map.py --diff <기준> <새> --table <후보>`): 새 열에서 해석 못 한 훅 자리(기능이 꺼짐), 옮겨진 자리, 새 빌드에만 있는 R12/R13 발견.
   - `emit`: 후보 `game_addresses_data.inl`(열 둘 `[기준, 새]`, 가드 해시 짝(`GH()` 리터럴의 기준판 해시 → 새 이미지 해시), 관찰 훅·준비 판정 12곳의 새 열)과 새 이미지의 정적 해석기 검사.
4. `report.md`를 읽고 ATTENTION·BLOCKED를 사람이 처리한다(필요하면 소스·가드 바이트를 고친다). 승격은 후보 표를 입력으로 한다:
   - `python -X utf8 tools/patch_next.py promote --target <새>`: 계획만 보여 준다(`addr_promote plan --table <후보>`, 아무것도 쓰지 않음). **실게임 확인**(`addr_build`가 새 빌드, `addr_unresolved`·기능 꺼짐 목록이 보고서와 같음) 뒤 `--confirmed`를 붙이면 후보를 `src/game_addresses_data.inl`에 두고 `addr_promote apply`를 돌린다(커밋 안 한 표 변경이 있으면 거부, 실패하면 표를 되돌린다).
5. 새 빌드를 쓰기로 정하면 승격한다(`promote --confirmed`가 `apply`를 부른다. 나머지는 모두 `--exe <새 해제본>`):
   - `addr_promote.py plan --from <이전> --to <새>`: 아무것도 쓰지 않고 id 대응, 삭제될 0 id, 소스 변경 건수를 보고한다.
   - `addr_promote.py apply …`: 표를 열 하나로 다시 쓰고(새 RVA로 id 이름, 같은 RVA 병합, 0 id는 소스가 이름을 안 쓰고 컨테이너도 아니면 삭제·아니면 `GA_X…`), 소스·테스트·도구·개발 json의 `GA_<옛>` → `GA_<새>`, `GH()` 해시(해시 짝으로), 테스트의 16진 RVA 리터럴, `ga_wild`(새 이미지 디코드)를 한 번에 바꾸고 변경 한 줄 한 줄을 `changes.tsv`에 적는다.
   - `addr_promote.py sweep --from-exe <이전 해제본> --old-table <git show <이전 커밋>:src/game_addresses_data.inl> --apply`: 새 코드에 없는 바이트 리스트(실행 경로가 없어 픽스처가 못 보는 설치용 가드 포함)를 이전 이미지에서 유일하게 찾아 새 위치의 바이트로 고친다. 이전 코드의 사본(바이트 그대로 복사본이 아닌 스텁)은 건드리지 않는다.
   - `addr_promote.py converge --exe …`: `ga_equiv_dump`를 돌려 기대 바이트가 이미지와 다른 실행 가드가 없을 때까지 반복해 고친다(앞 가드가 뒤 가드를 가리므로 여러 번 돈다).
   - `addr_promote.py literals --old-table … [--suffixed-only | --interior]`: 테스트 리터럴을 따로 다시 고친다(`u`/`ull` 접미사, id + 작은 오프셋). 한 번 바꾼 값은 다시 바꾸지 않는다(새 RVA 값은 건너뜀).
6. 증명: `tools/build_ga_equiv_dump.ps1`로 덤프를 뜨고 `tools/ga_equiv_compare.py <승격 전 덤프> <승격 후 덤프> --nonaux …`가 `EQUIVALENT`여야 한다(순번 없이 RVA·기대 바이트·해시 범위·읽은 값·거부 파일이 같다). `addr_promote.py check --exe … --dump …`가 GH를 현재 EXE에서 직접 계산한 FNV와 대조한다. 덤프에 안 나오는 가드는 `sweep`이 정적으로 덮는다.
7. 필수 검사 `tools/check_feature.ps1 -Feature dll-split -Build`, 실게임 확인(`addr_build`·`addr_unresolved 0`·기능 꺼짐 목록 동일) 뒤 커밋.

## 이번 재기준 결과 (2026-11-07, W1-E — 실게임 미확인)
- id 2,071개 → 2,057개(삭제 14, 거부 유지 2, 병합 0). id 토큰 3,785곳, 해시 236곳, 테스트 리터럴(정확 1,778 + 접미사 78 + 인접 19), 가드 바이트 리스트(덤프 경로 + 스윕)를 고쳤다. 전법 우선순위 파일 6개는 W1-E에서 제외했고 W8에서 새 id로 직접 고쳤다(별칭 20개 삭제).
- 덤프 비교: 승격 전(`development/ga_equiv/baseline_b1_data`, 데이터 영역 가드 2줄 보충)과 승격 후 `EQUIVALENT`. 1-F로 지운 이전 빌드 가드의 흔적만 `--ignore-single modpack_clamp_restore_safe --ignore-line "V 231AFB"`로 명시했다.
- 해시: 프로필 행 262개와 덤프 해시 115줄이 현재 EXE의 FNV와 같다. 가드 리터럴: 덤프 174줄 전부, 정적 `ga::mcmp` 69곳 전부 이미지 바이트와 같다.

## 도구 (W2, 2026-11-07)
- `tools/pelib.py`: PE 읽기, 와일드카드를 만드는 x86-64 길이 디코더(`ga_wild`와 같은 마스크: 현재 표의 1,389개 코드 id 마스크가 전부 일치함을 확인), 이미지 레지스트리(빌드 이름 = EXE sha256 앞 8자리 또는 경로, `build/ui_native_analysis/*/*.unpacked.exe`). 퇴역한 `rva_remap.py`의 디코드 부분을 대신한다.
- `tools/gen_signatures.py analyze --ref current --target <새>`: 위 `resolve`의 본체. `addid <RVA…> [--apply]`는 새 원작 주소를 쓰고 싶을 때 `GA_<RVA>`를 표에 넣는다(기준판 이미지에서 종류·방법을 도출, `ga_users` 비트맵은 소스에서 채움).
- `tools/gen_emit.py --target <새> --out <파일> [--check]`: 후보 표 방출기(`ga_table.emit` 사용). `--target current --check`는 기준판 이미지만으로 다시 만든 표가 커밋된 표와 바이트 단위로 같음을 보인다(자체 검사의 일부).
- `tools/hook_map.py --diff <기준> <새> [--table …]`, `tools/addr_promote.py plan --table …`: patch_next가 부르는 쪽.
- 자체 검사: `python -X utf8 tools/patch_next.py selftest`(기준 = 대상, 24초 안팎, 검사 경로 `patch-next`): id 전부 OK, 후보 표 = 커밋된 표, detect, 훅 지도 diff 0, C++ 해석기 대조 1,885개 불일치 0, 기능 게이트, 보고서 `ALL OK`.

## 리허설 결과 (2026-11-07, 옛 빌드 1fbe6be7 해제본을 "다음 빌드"로 — 실게임과 무관)
`python -X utf8 tools/patch_next.py run --target 1fbe6be7` (1분 안팎, 스테이지 픽스처 빌드 포함). 보고서: OK 10 / AUTO 1,773 / ATTENTION 122 / BLOCKED 160 / DENIED 2(id 2,067 = 2,057 + 컨테이너 id 10). 새 방법 23개 도출, C++ 해석기와 불일치 0(1,902 대조).
- 타당성: W1-E 때 `rva_remap` + B 단계 수작업 판정이 정한 대응(`development/addr_promote/1fbe6be7_to_1a4a447d/idmap.tsv`, 역방향)과 대조하면 값을 낸 id 1,902개 중 1,898개가 같다(AUTO 1,771, ATTENTION 117, OK 10). 다른 3개: `GA_7AB6A0`(도출 패턴이 순서를 벗어남 → ATTENTION으로 잡힘), `GA_7B0DD0`(표 전용 id, ATTENTION으로 잡힘), `GA_5077A0`(도출 패턴이 인접한 쌍둥이 함수를 맞춤, AUTO로 통과 — 알려진 한계: 본문이 같은 쌍둥이는 도구가 못 가린다). 못 찾은 160개(BLOCKED)는 옛 수작업이 값을 정해 준 id들이고, 도구는 정직하게 "사람이 정할 일"로 남긴다.
- 가드: 실행되는 가드 290개 중 OK 159, RELOC 99(재배치 칸만 달라 런타임 가드가 통과 못 하는 곳, 와일드카드 확장 후보), CHANGED 4(`native_awards.inl:261`의 가드 바이트 2곳, 해시 행 `GA_813940`·`GA_876AD0`), UNMAPPED 28(주변 id가 없음). 훅 지도: 해석 못 한 자리 41, 새 빌드에만 있는 발견 36(오류 0). 빌드 게이트 표는 convenience 29개·combat 8개 기능이 꺼지는 것으로 나온다(대부분 BLOCKED id 때문). 이 숫자는 도구가 그럴듯한 ATTENTION을 내는지 보는 용도이며 옛 빌드를 다시 지원한다는 뜻이 아니다.
- 한계: 해제본이 `build/ui_native_analysis/` 밖으로 옮겨지면(`backup/old_build_1fbe6be7/`) `--target <경로>`로 준다.

## 지켜야 할 것
- 서명 검색은 우리 DLL 안에서 읽기만 한다. 실행 중 원작 코드는 Layout 관리자만 쓴다.
- 다른 모드가 먼저 패치한 지점은 서명이 일치하지 않을 수 있다. 서명은 그 지점이 아니라 함수 시작이나 앵커로 찾고, 지점의 바이트 검사는 기존 guard와 체인 정책을 따른다.
- 새 원작 주소를 쓰려면 `GA_<RVA>`가 표에 있어야 한다(없으면 `python -X utf8 tools/gen_signatures.py addid <RVA> --apply`로 넣고 `tools/check_addresses.py`를 돌린다). 기능 코드에 숫자를 쓰지 않는다.
- 옛 빌드 흔적(`rva_remap`(퇴역·삭제), 쌍 분석, `b_verdicts`, `expected_disabled.json`, `rva_rebase`, `guard_rebase`)은 이 흐름에서 쓰지 않는다. `addrlib.py`는 소스 상수 스캔(`check_addresses (b)`) 전용으로 줄었다.
- `addr_promote`는 `development/addr_promote/` 기록 폴더를 다시 쓰지 않는다(이전 승격의 `GA_` 이름은 역사다). 리허설 `plan`이 이 결함(기록의 옛 이름 2,076개가 "모르는 토큰"으로 나옴)을 드러내 고쳤다.
