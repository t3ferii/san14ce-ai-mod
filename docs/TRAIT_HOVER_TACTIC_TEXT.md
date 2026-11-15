# 작업서: 전법 마우스오버 문구를 게임 데이터로 만들기 (TXT 제거)

- 상태: 구현됨(T1·T2), 실게임 미확인. 조사는 xhigh 계획, 2026-11-06. REA·게임 실행 없음.
- 근거: `tools/ghidra_full.py`(새 빌드 기본)·`index.sqlite`, 두 빌드 unpacked EXE 읽기 전용 해독, `research/rva_map/1fbe6be7_to_1a4a447d/functions.tsv`·`data_refs.tsv`.
- `HISTORY.md` §1.8의 전법 TXT 경로를 대체한다.

## 0. 결론 요약
1. 개성의 `3594B0` 같은 "전법 id → 설명 한 줄" 함수는 원작에 없다. 대신 원작 전법 목록의 **열 서술자 표**(옛 `1A3A6A0`, 새 `1A3D680`)가 있고, 첫 탭이 TXT 머리말의 '특징' 탭이다. 열 `{이름, 계통, 의존능력, 대거점, 효과1, 효과2}`가 TXT와 같다.
2. 열마다 `const wchar_t* __fastcall fmt(CTacticsData*)` 서식 함수와 머리글 메시지 id가 있다. 서식 함수 5개를 `world+0x774D0+i*8` 전법 객체에 직접 부르고 머리글은 `30F010(id)`(옛 30CF80)로 얻는다.
3. 서식 함수 5개는 두 빌드 모두 `same`, 읽기·문자열 표 조회만 한다(부작용 없음).
4. TXT 폴백은 두지 않고 TXT 경로를 완전히 지운다(§2.6).
5. 새 GA id 6개(함수 5, 데이터 1). 모두 `same`/`same_function_ref` → verdict 불필요. 가드: 해시 5행 + 열 서술자 대조(stage 7).
6. 단계: Sonnet T1, T2, 문서 T3, Claude 빌드·설치.

## 1. 원작 근거

### 1.1 특징 탭 = CTacticsData 목록 열 서술자 표
- 열 서술자(0x60바이트, 기존 hover의 `desc+0x18`·`desc+0x5C`와 같은 구조): `+0x00` 열 번호, `+0x10` 폭·정렬, `+0x18` 셀 서식 함수, `+0x28` 정렬 비교, `+0x48` 머리글 메시지 id, `+0x5C` 사용 여부.
- 탭 표(0x18바이트): `{int* 열목록, 0, 탭 제목 id}`, `-1` 종료.
- 표를 쓰는 곳(모두 `same`):

| 새 RVA (옛) | 역할 | 탭 표 | 제목 |
|---|---|---|---|
| 4A9A70 (4A76B0), 4A9EF0 (4A7B30) | 전법 선택 대화상자 | 1A3DC80 (탭0 제목 0x2DC) | `30F010(0x5BF)` |
| 7CB040 (7C8B90) | 전법 정보 목록 | 1A3DCE0 (탭0 제목 0x3CD) | `30F010(0x5BF)` |
| 619A50 (617200) | 열 15 변형 | 1A3DD40 | |

- 탭별 열: 탭0 `{0,1,2,3,12,13}`, 탭 0x404 `{0,4,5,6,7}`, 탭 0x405 `{0,8,9,10,11}`.
- 셀 문구: `4A67A0`이 `text=(*(desc+0x18))(row)` 후 `9333C0(label,text)`. ABI 확인.

### 1.2 특징 탭 다섯 열

| 열 | 머리글 id | 옛 → 새 | 크기 | 내용 |
|---|---|---|---|---|
| 1 계통 | 0x455 | 365FD0 → 3684C0 | 9B leaf | `movzx ecx,byte[rcx+40h]; jmp 30CE80→30EF10` (계통 1..9 → 이름 표 `0x147+계통`) |
| 2 의존능력 | 0x456 | 356F10 → 3592F0 | 215B | +0x6A: 0 "", 1 `30F010(0x239)`, 2 `30F010(0x23A)`, 그 밖 swprintf |
| 3 대거점 | 0x4B4 | 365FE0 → 3684D0 | 114B | +0x44 bit0 → `30F010(0x338)`(○) / `0x339`(×) |
| 12 효과1 | 0x404 | 3586C0 → 35AAA0 | 102B | +0x62 → `30EED0(e)` = 이름 표 `0x151+e` (1..0x17) |
| 13 효과2 | 0x405 | 358C90 → 35B120 | 102B | +0x63 → `30EED0(e)` |

- 열 0(이름) `35A9F0→363720` = `return t+0x10`. 현재 `name_key` 포인터 동일성 판정과 일치. 무장 열 `366CF0`도 `world[0x774D0+person[0x52]*8]+0x10`.
- 두 unpacked EXE에서 열 1,2,3,12,13의 `+0x00/+0x18/+0x48`이 위 표와 같다(+0x5C=1).

### 1.3 인자·반환·전제·부작용
- 인자 rcx = `CTacticsData*`(`world+0x774D0+i*8`, 크기 0x90, `22FB30(t)=(t-첫항목)/0x90`).
- 반환: 게임 문자열 표 안의 `const wchar_t*`. 값 없으면 빈 문자열(null 아님).
- `t[0xF]-t[0xE]==0x28`이면 `vt+0x48`(슬롯 9 `DA60`, 순수 복호 계산).
- 전제: UI 스레드, 시나리오·메시지 표 로드 후(tick 훅 위치, `3594B0`과 같음). `32F500(t)` 참일 때만.
- 부작용: 없음. 예외는 `3592F0`의 의존능력 0/1/2 외 분기가 공용 임시 문자열 링(`1D3BC0`)을 한 칸 전진(원작 목록과 같은 동작, 즉시 복사). TXT 126줄은 모두 무력/지력이라 실제로 안 탐. 사용 가능.
- 머리글 id가 `30F010`용이라는 근거(정황): `89E5E0`이 대거점 라벨에 `30F010(0x4B4)`; 0x404/0x405는 효과 탭 제목; 0x239/0x23A가 열 0 제목 0x234 근처. 머리글 그리는 코드는 못 찾음(§6).

### 1.4 수치 치환
- 특징 탭 다섯 열은 고정 문자열, 수치 없음. 수치 열(효과 탭 `35B410` 등, Full 말풍선 0xD 라벨)은 이번 범위 밖.

### 1.5 기각
- 원작 `CTacticsBalloonFullLayout`(vt 새 147A930) 재사용: 계통 없음, 전용 리소스·부모 패널 필요, 수명 관리가 다름.
- 열린 목록 서술자 실시간 읽기: 화면·탭 상태에 의존.

### 1.6 TXT와 일치
- 126줄 모두 `계통 / 의존능력 / 대거점` + `효과1` + 선택 `효과2`(효과2 있음 108, 없음 18). 계통 8종·효과 22종 모두 범위 안.

## 2. 설계

### 2.1 흐름 (키 `0x200|i`)
1. `name_key`가 포인터 동일성으로 i를 찾는다(현행).
2. `t=world[0x774D0+i*8]`, `32F500(t)`로 유효 확인.
3. 서식 함수 5개와 머리글 `30F010(id)` 5개 호출.
4. 순수 함수 `compose_tactic_text` → `8A2140(tip,text)`(복사함, 지역 wstring 가능).

### 2.2 문구 형식
```
<0x455>: <v1> / <0x456>: <v2> / <0x4B4>: <v3>
<0x404>: <v4>
<0x405>: <v5>
```
- 값이 비면 그 조각·줄을 뺀다. 줄 1은 첫 조각 다음에만 `" / "`.
- 머리글 null이면 `": 값"`.
- 1023자, `cut_wide`로 서러게이트 보호.
- 계통(v1)이나 효과1(v4)이 비면 빈 문구(전법 0·미사용 칸 거름).

### 2.3 `trait_hover_profile.h` 추가 (순수, 픽스처 대상)
```cpp
struct TacticField {unsigned ga_id,caption_id,column;};
static const unsigned kTacticFields=5,kTacticColStride=0x60,kTacticColFormatter=0x18,kTacticColCaption=0x48;
static const TacticField kTacticFieldTable[kTacticFields]={   // CTacticsData list feature tab (old 1A3A6A0 / new 1A3D680)
    {GA_365FD0,0x455,1},{GA_356F10,0x456,2},{GA_365FE0,0x4B4,3},{GA_3586C0,0x404,12},{GA_358C90,0x405,13}};
inline unsigned tactic_func_rva(unsigned k);     // k>=5 -> 0
inline unsigned tactic_caption_id(unsigned k);
inline bool tactic_column_ok(const unsigned char *desc,unsigned column,uint64_t fn_va,unsigned caption); // +0x00/+0x18/+0x48 (memcpy)
inline std::wstring compose_tactic_text(const wchar_t *const *caption,const wchar_t *const *value);     // 2.2
static const unsigned trait_hover_tactic_columns_rva=GA(GA_1A3A6A0);
```
- `+0x5C`는 원작이 실행 중 바꿀 수 있어(`4D1930`) 대조하지 않는다.

### 2.4 `native_trait_hover.inl` 변경
```cpp
static std::wstring tactic_text(Obj t,unsigned *empty_mask=nullptr) {   // edit_text와 같은 꼴
    const wchar_t *caption[thp::kTacticFields],*value[thp::kTacticFields];
    for(unsigned k=0;k<thp::kTacticFields;++k) {
        caption[k]=fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30CF80))(thp::tactic_caption_id(k));
        value[k]=fn<const wchar_t *(__fastcall *)(Obj)>(thp::tactic_func_rva(k))(t);
    }
    // mask: bit k = 값 빔, bit 8+k = 제목 빔
    return thp::compose_tactic_text(caption,value);
}
```
- `name_key`: `has_text=tactic_text(root,ti)!=nullptr` → `Obj t=tactic_object(root,ti); has_text=valid(t) && !tactic_text(t).empty();`. `name_key_decide`는 그대로.
- `show_impl`: KC_TACTIC이면 `t=tactic_object(root,idx)`, `!t||!valid(t)`면 fail=1. 키가 바뀔 때만 `text=tactic_text(t,&mask)`; 비면 `hide_tip` + `note_text_failed(3,…)`; 아니면 `set_text`. `const std::wstring *tactic` 변수 삭제.
- `guards_ok`: `ids[]`에 `trait_hover_tactic_columns_rva`와 `tactic_func_rva(0..4)` 추가(stage 5). 해시 루프 다음 stage 7: 다섯 필드마다 `tactic_column_ok(image+cols+col*0x60, col, (uintptr_t)image+tactic_func_rva(k), caption)`, 실패 시 `guard_fail(7,k,cols)`.

### 2.5 다른 언어 EXE
- index는 포인터 동일성, 문구는 index → 객체 → 원작 서식 함수 → 그 언어의 표. 구분자 `": "`, `" / "`, `"\n"`만 고정.

### 2.6 TXT 폴백 없음 (완전 삭제)
- 이유: TXT는 v12.20부터 미배포; 원작 함수를 못 쓰는 경우(GA 미해석·해시 불일치)는 hover 전체가 가드로 꺼짐; TXT는 한국어 이름 비교라 다른 언어에서 원래 안 맞음.
- 지울 것:
  - `native_trait_hover.inl`: `tactic_txt`, `tactic_txt_tried`, `load_tactic_txt()`, 옛 `tactic_text()`, `trait_hover_tactic_txt` 기록
  - `trait_hover_profile.h`: `kTactTxtLimit`, `TxtStatus`, `parse_tactic_txt`, `thp::tactic_text` (헤더의 `MultiByteToWideChar` 의존도 사라짐)
  - `tests/trait_hover_fixture.cpp`: `test_txt`와 그 전용 도우미
- 남길 것(총괄 판단): `SplitManage.ps1`(281/289/385), `tools/release_current.py`, `tests/verify_share.py`, `tools/generate_handoff.py`, `docs/FOLDER_LAYOUT.md`의 TXT 보존·정리 처리. 옛 설치본의 사용자 파일을 다루는 공용 설치기 코드다.

### 2.7 가드 실패 범위
- 새 해시 5행·stage 7도 기존 가드 표에 넣어 hover 전체를 함께 켜고 끈다. 전법만 끄는 플래그는 만들지 않는다.

## 3. GA id, 가드, verdict

### 3.1 새 GA id 6개
| 옛 → 새 (예상) | 뜻 | 근거 |
|---|---|---|
| 365FD0 → 3684C0 | 계통 열 (leaf 9B) | functions.tsv `same L0 leaf`, 0xCC 뒤 16바이트 정렬이라 K_FUNC |
| 356F10 → 3592F0 | 의존능력 열 | `same L0 pdata` |
| 365FE0 → 3684D0 | 대거점 열 | `same L0 pdata` |
| 3586C0 → 35AAA0 | 효과1 열 | `same L0 pdata` |
| 358C90 → 35B120 | 효과2 열 | `same L0 pdata` |
| 1A3A6A0 → 1A3D680 | 열 서술자 표 (data) | data_refs `same_function_ref` (참조 함수 4A76B0, 4A7B30, 617200, 7C8B90 모두 same) |

- verdict 불필요. 재생성 값이 다르면 멈추고 functions.tsv/data_refs 근거로 `b_verdicts.json`에 더한다.
- 참고 모드가 이 6곳을 건드리지 않음(`gamerefs`로 확인).
- `tools/gen_signatures.py`의 `_trait_hover_ids()`: `tactic=[0x365FD0,0x356F10,0x365FE0,0x3586C0,0x358C90,0x1A3A6A0]`, `out[v]`에 `th_inl`과 `th_h`를 함께 귀속(guard_ids와 같은 처리). 그다음 `python -B -X utf8 tools/gen_signatures.py`로 재생성.
- **GH 리터럴을 헤더에 먼저 넣고 재생성한다**(`find_guard_hashes`가 src 리터럴을 읽음).

### 3.2 해시 행 (`trait_hover_guards[]`, 24행 → 29행)
| id | 크기 | 옛 GH (소스 리터럴) | 새 빌드 예상 |
|---|---|---|---|
| 365FD0 | 0x09 | 0x836D5B8137481FFA | 0xD2DA04B86B5D8ABE |
| 356F10 | 0x40 | 0x152C588A0AD17874 | 0x7110CD2EC7AD7A2F |
| 365FE0 | 0x40 | 0x236B7AE8CBB7C17A | 0x9AAD1D8CFFDAB6E8 |
| 3586C0 | 0x40 | 0x5DACB979EEA32D88 | 0xAC1D85DF64262211 |
| 358C90 | 0x40 | 0xB714E8FC5887DC66 | 0x0C48C3A479021BAC |

- `hash_ok`와 같은 FNV-1a 64. 기존 쌍 30CF80·3594B0을 같은 스크립트로 재계산해 일치 확인.

### 3.3 stage 7
- 열 1,2,3,12,13마다 `+0x00==열번호`, `+0x18==image+GA(서식함수)`, `+0x48==머리글 id`. 실행 중과 두 빌드 픽스처 모두에서 보장.

## 4. 파일, 검사, 진단

### 4.1 파일
- `tools/gen_signatures.py`, `src/game_addresses_data.inl`(재생성, 공용)
- `src/trait_hover_profile.h`, `src/native_trait_hover.inl`, `tests/trait_hover_fixture.cpp`
- 문서: `docs/HISTORY.md`(§0-4, §1.3, §1.4 name_key 3단계, §1.8, §1.10, §3.3, §4-1, §5, §6.1), `docs/FEATURE_SWITCHES.md:99`, `docs/HISTORY.md:648,655,666`

### 4.2 검사
- 픽스처:
  - `compose_tactic_text`: 돌격 `{계통,의존능력,대거점,효과1,효과2}/{공격,무력,×,피해,적 사기저하}` 결과가 TXT 돌격 줄(`\n` 치환)과 같음; 분전(효과2 "") 2줄; 의존능력 ""이면 `" / "` 연속 없음; v1 ""·v4 null·v4 ""이면 빈 문구; 머리글 null이면 `": 값"`; 1023자·서러게이트 경계.
  - `kTacticFieldTable`: 열·머리글 순서, rva 0 아님·서로 다름.
  - `tactic_column_ok`: 가짜 0x60바이트로 정상/열 번호/함수/머리글 틀림.
  - `test_guards` 29행, `name_key_decide` 설명을 "game text 없음"으로.
- 명령: `check_feature.ps1 -Feature trait-hover`, `-Feature address-table`(두 빌드 trait_hover track, stage 7·해시 포함), `tests/verify_hook_scopes.py`(기존 8건 외 새 실패 없음), `tools/check_observer_hooks.py`.

### 4.3 진단
| 이벤트 | army / value / hex | 빈도 |
|---|---|---|
| `trait_hover_tactic_text` (새) | 전법 index / 문구 길이 / 빈 칸 비트(bit0..4 값, bit8..12 제목) | 처음 8회 |
| `trait_hover_text_failed` 사유 3 (뜻 바뀜) | 전법 문구 비었음(계통·효과1 없음) | 기존 상한 16 |
| `trait_hover_guard_rejected` stage 7 (새) | 7 / 필드 k / 표 rva | 첫 실패 1회 |
| `trait_hover_tactic_txt` | 삭제 | |

## 5. 단계
- **T1** 주소·가드·순수 함수(삭제 없음): gen_signatures.py, profile.h(GH 5행, columns rva, §2.3), 재생성, 픽스처 새 시험. 완료: 새 id 6개·GH 5쌍이 §3.1/§3.2와 같음, address-table·trait-hover 픽스처 통과. 다르면 멈춤.
- **T2** 실행 코드 전환·TXT 제거(T1 다음): §2.4, stage 7, 진단, TXT 삭제. 완료: §4.2 명령 통과, `rg "전법설명|parse_tactic_txt|tactic_txt" src tests` 결과 없음.
- **T3** 문서(T2와 병렬): §4.1 문서. 실게임 미확인 표시.
- **최종(Claude)**: build_split -RuntimeDiagnostics → dll-split -Build → install_local local13 → generate_handoff.
- 실게임 확인: 전법 선택 화면·전법 정보 목록 이름 셀(특징 탭과 같은 문구), 무장 목록 전법 열, 콜라보 전법, "없음" 셀 말풍선 없음, `trait_hover_tactic_text` 빈 칸 비트 0.

## 6. 위험과 미확인
| 항목 | 내용 |
|---|---|
| 머리글 문자열 내용 | 0x455="계통" 등은 열 의미로 추정. 같은 열 서술자 `+0x48` 값이므로 원작 목록 머리글과 같은 id. 실게임 확인 항목. |
| `3592F0` 예외 분기 | 임시 링 한 칸, 실제로 안 탐, 상태 변경 없음 |
| 공용 생성 파일 | `game_addresses_data.inl` 재생성은 다른 작업과 순서 맞춤 |
| 프레임 비용 | 전법 셀 위에서 프레임당 원작 함수 10회 |
| 설치기 TXT 처리 | 남김, 총괄 판단 |
