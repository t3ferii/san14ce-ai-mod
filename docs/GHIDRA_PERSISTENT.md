# 삼국지14 CE KO 영구 Ghidra 조회

현재 기준판은 Steam buildid 25953386, `1a4a447d…`다. `ghidra_full.py`, `decomp_cache.py`, `ghidra_persistent.ps1`의 기본 대상은 모두 이 기준판이다. 과거 분석의 완료 수치·재열기 실험은 [HISTORY](HISTORY.md)에 보존하며 현행 검증 결과로 쓰지 않는다.

## 먼저 사용할 자료

[원작 통합 조사본](../../San14Sim/docs/orig/README.md) → 해당 종류 문서·origlib → 저장한 함수 추출본 순서로 찾는다. 원작 EXE에 REA를 새로 열거나 전체 재분석하지 않는다. 게임 루트에서 다음을 사용한다.

```powershell
python -X utf8 AI_TacticsMod/tools/ghidra_full.py func 862030
python -X utf8 AI_TacticsMod/tools/ghidra_full.py callers 862030
python -X utf8 AI_TacticsMod/tools/ghidra_full.py uiclass CSetUpScenarioSenior
python -X utf8 AI_TacticsMod/tools/decomp_cache.py get 862030
```

`research/ghidra_full/1a4a447d14e68c40/`의 TSV·샤드와 신선한 `index.sqlite`를 읽는다. 색인이 없어도 TSV로 같은 조회를 한다. 분석·DB 재열기가 필요한 작업이 아니다.

## 저장 DB의 주소 제한 내보내기

추출 실패 함수나 asm·바이트가 필요할 때만 주소를 모아 한 번에 읽는다.

```powershell
& ./AI_TacticsMod/tools/ghidra_persistent.ps1 -Action Status
& ./AI_TacticsMod/tools/ghidra_persistent.ps1 -Action Export -Address 862030,9309C0
```

기본 DB는 `C:\Users\<user>\Documents\Codex\analysis\SAN14CE_KO\1a4a447d14e68c40\SAN14CE_KO.gpr`다. `Status`는 manifest만 읽고, `Export`는 저장 DB를 `-noanalysis -readOnly`로 열어 필요한 주소의 C·asm만 내보낸다. 원본 EXE와 Program을 수정하지 않으며, 결과는 해당 해시의 `research/decomp_cache/`에 없는 파일만 추가한다. 설정된 원본 해시·DB 완료 상태가 맞지 않으면 중단한다.

`FullExport`는 추출본을 새로 만들라는 명시적 작업이 있을 때만 쓴다. 완료 후 `tools/ghidra_index.py`를 한 번 실행한다. `Initialize`는 기존 DB를 덮어쓰지 않으며 일상 조회에 사용하지 않는다. `Verify`는 과거 빌드의 두 표본 함수에 고정된 역사 검증이므로 현행 검사는 `tools/check_addresses.py`를 사용한다.

과거 빌드 조회는 명시적인 `--build <해시 앞 8자리>` / `-Build <해시 앞 8자리>`로만 선택한다. 당시 주소·분석 근거를 현재 RVA처럼 사용하지 않는다. 빌드별 대상·해시와 이력은 [HISTORY](HISTORY.md), 일반 규칙은 [SESSION_RULES §7](SESSION_RULES.md)을 따른다.

## 참고 모드 DLL 추출본 (2026-11-05)

- 별도 영구 프로젝트 `C:\Users\<user>\Documents\Codex\analysis\SAN14_refs\SAN14_refs.gpr` (원작 DB와 무관). 추출: `ghidra_persistent.ps1 -Action RefExport [-Only <이름 일부>] [-Force]` (DLL마다 import+분석+`San14RefExport.java`, 이미 complete면 건너뜀).
- 병렬(2026-11-05): `-Parallel N`(생략 시 min(작업 수, 코어/2, 여유 RAM GB/3)). DLL마다 별도 프로젝트 `SAN14_refs\jobs\<folder>\<folder>.gpr`(+잠금·로그·스크립트 폴더)로 analyzeHeadless를 동시에 띄우고, 큰 DLL부터 시작한다. 작업당 `-max-cpu`=코어/N, 힙=여유 RAM*0.75/N(2~6GB)이며 `-MaxCpu`/`-Heap`로 고정할 수 있다. `index.tsv`는 전부 끝난 뒤 한 번 쓴다. 전체 재추출: `-Action RefExport -Force -Parallel 4`(약 30분대, 순차 약 87분). 메모리가 부족하면 `-Parallel`을 줄이고 실패한 것만 `-Only`로 다시 돌린다. 옛 단일 프로젝트 `SAN14_refs.gpr`는 더 쓰지 않는다.
- 결과 `research/ghidra_refs/<이름>_<sha16>/` (원작 추출본과 같은 TSV + `exports.tsv`, `game_rvas.tsv`), 목록 `research/ghidra_refs/index.tsv`.
- 조회: `python -X utf8 tools/ghidra_full.py --ref Traits9_UI exports`, `--ref all grep <패턴>`, `--ref FogTrial str <패턴>`, `gamerefs 86E320` (원작 주소를 상수로 가진 참고 모드 함수). 참고 모드 주소는 `180001000` 또는 RVA. 이 조회들은 `tools/ghidra_index.py`가 만든 `index.sqlite`가 있으면 그것을 쓴다([SESSION_RULES §7](SESSION_RULES.md)). `uiclass`/`uislot`/`uimap`은 원작 빌드의 RTTI·vtable 슬롯 표를 읽는다.
- `game_rvas.tsv`는 즉치/데이터 값이 원작 함수 시작과 같은 후보(RVA 0x2000 이상)다. 우연한 일치가 섞이므로 loc의 함수 C로 확인한다.
- `gamerefs`는 game_rvas(함수 시작·데이터 값 일치)와 참고 모드 C의 `0x<주소>` 리터럴을 함께 찾는다(약 20초). 함수 중간 주소는 포함 함수 시작도 같이 본다. 디컴파일되지 않은 코드/패턴 스캔 훅은 못 찾는다.
