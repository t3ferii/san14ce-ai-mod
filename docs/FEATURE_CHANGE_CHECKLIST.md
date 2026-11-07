# 기능 추가·수정 필수 체크리스트

새 기능(모드)을 만들거나 기존 기능의 진입점·설정·의존성을 바꿀 때 **같은 변경 안에서** 아래를 모두 처리한다. W9 모드 매니저·설치기 종속 작업 뒤에 생긴 규칙이며, `feature_deps`·`gen_installer_catalog`·`verify_installer_gui` 검사가 이 문서의 항목을 확인한다.

## 1. 기능의 진입점을 먼저 정한다

- 기능을 어디서 여는지 적는다: 지도, 도시 메뉴, 군사보고, 설치기, 자동 AI 등.
- **다른 기능의 UI/버튼으로만 열린다면 그 기능은 부모 기능의 자식이다.** 예: 부동산 개발은 군사보고의 「부동산 개발」 버튼으로만 열리므로 군사보고의 자식이다.
- 기능이 꺼졌을 때 진입점도 사라지는지, 부모가 꺼지면 자식도 꺼지는지 확인한다.

## 2. 네 곳을 같은 방향으로 맞춘다

| 위치 | 무엇을 적나 | 검사 |
|---|---|---|
| `development/features.json` | 모듈 `depends`(빌드·코드 의존), `owns`/`sources` | `gen_module_manifests --check`, `mod_compat` |
| `development/modules/<id>/module.json` | 위에서 생성된 모듈 매니페스트 | `gen_module_manifests --check` |
| `development/installer_map.json` | 플레이어 설정 종속 `requires`(부모 키), 필요할 때만 `inherit` | `gen_installer_catalog --check` D9 |
| `src/native_adapter.cpp` | `f.자식 = f.부모 && feature_key(config, ...)` | `feature_deps --check` D1 |

- **설치기 `Requires`와 모듈 `depends`는 같은 방향이어야 한다.** D9가 불일치를 실패시킨다.
- **DLL의 실제 코드 의존 방향도 모듈 `depends`와 같아야 한다.** D1이 새 역방향 엣지를 실패시킨다.
- UI만의 종속이라도 설치기 `Requires`와 모듈 `depends`에 적는다. 코드 의존이 반대 방향이면 공용 상태를 중립 헤더나 부모 모듈 소유 헤더로 옮겨 순환을 만들지 않는다. 예: `src/advisor_terrain_bridge.h`는 advisor-briefing 소유, terrain-edit이 include한다.
- `inherit`은 부모 스위치 값을 그대로 물려받는 좁은 경우에만 쓴다. 일반 종속은 `Requires`다.

## 3. 생성기와 검사를 순서대로 실행한다

```powershell
python -B -X utf8 tools/gen_module_manifests.py --apply
python -B -X utf8 tools/gen_installer_catalog.py --apply
python -B -X utf8 tools/gen_module_table.py --apply

python -B -X utf8 tools/gen_installer_catalog.py --check   # D9: 설치기 Requires ↔ 모듈 depends
python -B -X utf8 tools/feature_deps.py --check             # D1/D10: 코드 엣지·순환
python -B -X utf8 tools/mod_compat.py --selftest
python -B -X utf8 tools/installer_preflight.py
python -B -X utf8 tests/verify_installer_gui.py
```

- 기능 경로도 함께 돌린다: `tools/check_feature.ps1 -Feature <기능> -List`로 고른 뒤 실행.
- 코드 변경이면 `tools/check_feature.ps1 -Feature dll-split -Build`까지 통과한다.
- 새 종속 쌍은 `tests/verify_installer_gui.py`의 `expected_requires`에 추가해 다시 깨지지 않게 한다.
- 진입점 UI를 바꿨으면 해당 fixture에 클릭·차단·부모 OFF 동작을 추가한다.

## 4. 문서도 같은 변경에 포함한다

- `docs/FEATURE_SWITCHES.md`: 해당 행의 `묶음 / 필요 기능`에 부모를 적는다.
- `docs/MOD_MANAGER_DESIGN.md`: 새 종속·예외·소프트 의존을 기록한다.
- `docs/ADVISOR_BRIEFING.md` 등 기능 문서: 진입점·화면 흐름·꺼졌을 때 동작.
- 릴리스 시 `docs/RELEASE_NOTES.md`, `docs/CHANGES_ALL.md`, 필요하면 README.
- `development/CURRENT.md`는 설치·검증 경계만 짧게 갱신한다.

## 5. 배포는 PUBLISH.md만 따른다

- `docs/PUBLISH.md`의 빌드·패키지·릴리스 순서를 그대로 따른다.
- 공개 DLL은 진단 없이 빌드하고(`-RuntimeDiagnostics` 금지), ZIP·`release_features.json` 두 자산을 확인한다.
- 배포 전 `build_share.py`와 `tests/verify_share.py`를 통과한다.

## 최근 실수 기록 (2026-11-08)

부동산 개발을 군사보고 버튼으로 옮겼는데 설치기 행은 최상위 `real_estate.terrain_edit`로 남겨 두었다. 그래서 보고를 꺼도 부동산 개발이 따로 켜질 수 있었다. 수정은 세 방향을 동시에 맞췄다.

- 설치기: `real_estate.terrain_edit` → `Requires='game_flow.advisor_briefing'`
- 모듈: `terrain-edit` → `depends`에 `advisor-briefing`
- DLL: `f.terrain_edit = f.advisor_briefing && feature_key(...)`
- 공용 arm 상태는 `src/advisor_terrain_bridge.h`(advisor-briefing 소유)로 옮겨 코드 엣지도 `terrain-edit → advisor-briefing`으로 맞췄다.
- `verify_installer_gui` 기대 목록과 `feature_deps` D1이 이 쌍을 검사한다.
