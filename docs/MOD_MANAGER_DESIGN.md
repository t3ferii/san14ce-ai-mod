# 모드 매니저 설계 (2026-11-08, W9-1)

사용자 목표(2026-11-08): "앞으로 모드 추가 많이 할거야. 다른 모더가 만든 모드와의 호환성도 많이 볼거고." 기존 계획은
`docs/MODULARIZATION_PLAN.md`의 "리팩토링 목표 구조"다. 이 문서는 그 목표를 **런타임 비용 0** 조건으로 구현하기 위한
설계와 이번 단계의 범위를 확정한다.

## 1. 불변식 (성능)

- **I-M1**: 매니페스트·카탈로그·의존성 계산은 빌드·설치 시점에만 쓴다. 게임 DLL은 매니페스트를 읽지 않는다.
- **I-M2**: 같은 기능 집합·같은 활성화 순서라면 매니저 도입 전후 **런타임 소스·생성 표·훅 지도가 동일**해야 한다(`git diff -- src` 없음, `gen_module_table.py --check`, `hook_map` 동일). 이 빌드는 PE 타임스탬프/PDB 때문에 같은 소스도 DLL SHA-256이 매번 달라지므로(2026-11-08 확인), 해시 동일성은 `/Brepro`를 켤 때만 쓰는 보조 조건으로 둔다.
- **I-M3**: 런타임에 새 훅·새 구독·핫 패스의 새 `log_event`를 만들지 않는다. `hook_map` 사이트 수·소유자·순서 해시가 그대로여야 한다.
- **I-M4**: 설치기 카탈로그 생성물은 `기능설정.ini` 키와 설치 항목만 만들고, 게임 설정 형식·세이브 형식·DLL 구성을 바꾸지 않는다.
- **I-M5**: 진단 빌드에서 프레임 비용(`perf_frame`, `layout_p4_built_cost`, `tactic_priority_perf`)이 노이즈 범위를 벗어나면 병합하지 않는다.

## 2. 모드 = 매니페스트 하나

경로: `development/modules/<id>/module.json` (schema 1). 기존 중앙 파일(`features.json`·`module_registry.json`)은
이번 단계에서 빌드 입력으로 유지하고, 매니페스트는 그 **모듈 부분의 단일 서술**로 생성·검사한다. 2단계에서 중앙 파일을
매니페스트에서 생성하는 방향으로 뒤집는다(8절).

```json
{
  "schema": 1,
  "id": "tactic-priority",
  "title": "tactic-priority",
  "host": "convenience",
  "build": "split",
  "status": "active",
  "hub": false,
  "alias_of": "",
  "depends": ["layout"],
  "conflicts": [],
  "owns": ["src/native_tactic_priority.inl"],
  "sources": ["src/native_tactic_priority.inl"],
  "tests": ["tactic-priority"],
  "hook_owners": ["tactic_priority"],
  "checks": ["tactic-priority", "split-module"],
  "ini": [{"section": "officer_ui", "key": "tactic_priority"}],
  "installer": {"items": [{"section": "officer_ui", "key": "tactic_priority"}]},
  "ui": [{"owner": "tactic_priority", "entry": "button"}],
  "provides": [],
  "uses": [],
  "hooks": [],
  "registry": {
    "rows": [],
    "subscriptions": [],
    "unity": [],
    "flags": []
  },
  "notes": ""
}
```

- `owns`/`sources`/`tests`/`depends`/`hook_owners`/`checks`는 현재 `features.json` 모듈과 같은 뜻이다.
- `ini`는 모듈이 읽는 `[section] key` 목록이다. 기존 모듈은 소유 파일 스캔으로 채우고, 새 모듈은 직접 적는다.
- `installer.items`는 설치기 항목이다. 설치기 키→모듈 매핑은 여러 허브 파일(`native_adapter.cpp`, `native_split_install.inl`)이
  모든 키를 읽어 자동 추론이 모호하므로, **새 모드는 명시**하고 기존 94개는 2단계에서 매핑표로 확정한다.
- `ui`는 Layout DLL의 UI 진입점 소유자다(버튼/팝업/토글). `layout_client::UiScope` 이름과 맞춘다.
- `hooks`는 다른 모더 모드처럼 코드가 아직 없는 매니페스트가 훅 자리를 미리 선언할 때 쓴다. 형식 `{"key": "GA_...", "len": 5}`.
- `registry`는 이번 단계에서 현재 `module_registry.json`의 모듈별 행을 그대로 옮긴 **거울**이다. 2단계에서 생성 방향을 뒤집는다.

## 3. 파이프라인

```
modules/<id>/module.json ──(1) 검증──┬─ features.json.modules (현재: 손 관리, 2단계: 생성)
                                     ├─ module_registry.json (현재: 손 관리, 2단계: 생성)
                                     ├─ src/module_table_*.inl (gen_module_table.py, 빌드 시)
                                     ├─ development/module_catalog.json (설치기·외부 도구용, 이번 단계 구현)
                                     └─ mod_compat 보고서 + 영향 검사 목록
```

- (1) `python -X utf8 tools/gen_module_manifests.py --check`: 매니페스트가 중앙 등록부와 일치하는지 검사(always 경로).
- `--apply`: 현재 중앙 등록부에서 매니페스트를 생성/갱신한다. **DLL을 다시 쓰지 않는다.**
- `python -X utf8 tools/mod_compat.py <id|경로>`: 새 모드/기존 모드를 다른 모드들과 비교한다.
- 설치기는 `development/module_catalog.json`을 읽어 의존성·충돌·설치 순서를 정한다(2단계).

## 4. 호환성 검사 (`mod_compat.py`)

대상 모듈 하나를 나머지 모든 매니페스트와 비교한다.

| 검사 | 근거 | 판정 |
|---|---|---|
| 의존성 | `depends`, `requires` | 없는 모듈·순환 = 오류 |
| 선언 충돌 | `conflicts` | 교집합 = 오류 |
| 같은 훅 자리 | `hook_map` 사이트의 `key`·`add`·vtable 슬롯 | 같으면 오류 |
| 같은 16바이트 블록 | 사이트 `rva // 16` | 겹치면 경고(원작 규칙 R2와 같은 기준) |
| 같은 원작 주소 쓰기 | 사이트 `original.ga`/`rva` | 교집합 = 오류 |
| API 충돌 | `provides` 이름 | 중복 제공 = 오류 |
| ini 키 중복 | `ini` + 소유 소스의 `feature_key` | 같은 `[section] key` = 경고(공유 설정일 수 있음) |
| UI 자리 중복 | `ui.owner`·entry | 같으면 오류 |
| 영향 검사 | `checks` + 역방향 `depends` | 충돌 모듈·의존자의 검사 경로를 출력 |

출력은 `텍스트 + --json`. 오류가 있으면 exit 1. 다른 모더 모드는 소스 없이 매니페스트만으로 비교할 수 있다(훅 자리·ini·UI·의존성).

## 5. 설치기 (모드 매니저)

이번 단계:
- `development/module_catalog.json`을 매니페스트에서 생성(기계 판독용: id, host, depends, conflicts, owns, checks, ini, installer).
- 설치기 3종 카탈로그와 모듈 카탈로그의 **키 집합 일치 검사**(누락·중복·미사용 키).
- 설치 전 `install_local.py`/`FeatureSelect.ps1`이 선택 조합의 의존성·충돌을 검사(게임 꺼진 상태).

2단계:
- 설치기 3종을 매니페스트에서 생성(손 편집 금지).
- 설치기 UI가 모듈 단위로 표시하고, 한 스위치가 UI 진입점과 실기능을 함께 켜고 끈다.
- `Requires`(설치기) ↔ `depends`(모듈) 매핑(D9)을 카탈로그에서 확정.

## 6. 새 모드 추가 절차 (목표)

1. `development/modules/<id>/module.json` 작성(소유 파일·호스트·의존·충돌·훅 자리·ini·UI·검사).
2. `python -X utf8 tools/mod_compat.py <id>` → 충돌 0 확인.
3. `python -X utf8 tools/gen_module_manifests.py --check` → 등록부와 일치.
4. (2단계) `python -X utf8 tools/gen_module_table.py --apply`로 중앙 등록부·카탈로그 생성.
5. 지정된 검사 경로만 실행. 새 코드는 host DLL의 유니티 슬롯에 들어가고, 런타임 추가 비용은 없다.

## 7. 마이그레이션 (기존 60모듈)

- `gen_module_manifests.py --apply`가 현재 `features.json` + `module_registry.json`에서 60개 매니페스트를 만든다.
- `--check`는 (a) 모든 모듈에 매니페스트가 있고 (b) 필드·소유 파일·검사·훅 소유자·registry 행/구독/유니티 항목이
  중앙 등록부와 같은지 확인한다. 불일치면 실패한다.
- 기존 동작·DLL 바이트는 바뀌지 않는다(런타임 코드 무변경).

## 8. 2단계(중앙 생성)로 넘어갈 조건

- 60개 매니페스트 `--check` 통과, `mod_compat` selftest 통과.
- 설치기 키→모듈 매핑표가 94개 항목을 모두 덮음(허브 파일의 광범위 읽기 때문에 자동 추론 대신 명시 매핑).
- 위 조건 전에는 `features.json`/`module_registry.json`을 계속 손으로 관리하고, 매니페스트는 거울로 둔다.

## 9. 합격 기준

- `gen_module_manifests.py --check`, `tests/verify_mod_compat.py`, `dll-split -Build` 통과.
- 런타임 코드 변경 0 → 같은 기능 집합에서 **런타임 소스·생성 표·훅 지도 동일**(DLL 해시는 빌드 비결정성 때문에 보조 조건).
- `hook_map` 553지점·경고 0, `feature_order` 순서 해시 `d0c6cb0b` 유지.
- 새 모드 예제에서 `mod_compat`가 훅·주소·ini·UI 충돌을 모두 잡고, 영향 검사만 출력.

## 10. 비목표

- 게임 실행 중 매니페스트 로딩, 동적 모듈 레지스트리, 프레임별 충돌 검사.
- 다른 모더의 설치기·바이너리 실행. 호환성은 매니페스트·훅 자리·문서 근거로만 판단한다.
- 새 DLL·EXE·설정 파일 추가.

## 11. 구현 상태 (2026-11-08)

- 1단계(매니페스트·호환성): 완료. `development/modules/<id>/module.json` 61개, `tools/gen_module_manifests.py`,
  `development/module_catalog.json`, `tools/mod_compat.py`(훅·주소·ini·UI·API·의존성/순환).
- 2단계(설치기 데이터·카탈로그): 완료. `development/installer_map.json` 94항목(키→모듈), C#/PowerShell 설치기 카탈로그를
  매핑에서 **생성**(GUI/PS 텍스트 차이는 `label_ps`/`note_ps`로 보존), 매니페스트의 `installer.items`·`ini` 자동 채움,
  `tools/installer_preflight.py`(현재 기능설정 → 모듈 의존성/충돌 검사, 0문제), D9 검사를 `gen_installer_catalog --check`로 수행.
- 3단계(설치기 종속성·설명 감사): 완료.
  - 모듈 단위 자동 켜기/경고는 넣지 않았다. 설치기의 기존 `Requires`/묶음 부모-자식 행이 플레이어 설정 종속을
    표현하는 정식 장치이고, `features.json`의 모듈 `depends`는 빌드·설치 순서/코드 의존성이라 UI 종속과 다르다.
    예: `tactic-priority`의 `trait-hover` 의존은 소프트 의존이다. 칸 마우스오버 설명은 `mod_features.tooltip_collab`이
    꺼져 있으면 소스가 그리기만 건너뛰고, 순위 지정·출진 기억은 그대로 동작한다(설치기 `Requires`로 묶으면 오히려
    툴팁을 강제하거나 기능을 숨기게 된다).
  - DLL의 feature-level AND 10쌍을 감사해 `diagnostics.ai_trace_original` → `diagnostics.ai_trace`와
    `real_estate.terrain_edit` → `game_flow.advisor_briefing`을 추가했고, 나머지 8쌍은 이미 설치기 `Requires`와
    일치했다. `Inherit` 2쌍(recruit_ai/injury_herbal)도 확인했다. 부동산 개발은 군사보고의 버튼으로만 들어가므로
    보고 기능이 꺼지면 지형 변경도 함께 꺼진다.
  - 설치기 설명 32개를 원작 구현·기존 문서와 대조해 플레이어용으로 다시 썼다(무엇이 바뀌는지, 조건/예외, 끄면 어떻게
    되는지). 비개발 항목은 30자 이상·내부 주소/훅 용어 없이 유지하는 검사를 추가했다(`installer_gui` 530검사).
- 아직(다음 단계 후보): `features.json`/`module_registry.json`을 매니페스트에서 생성하는 방향 전환, 다른 모더 패키지의
  매니페스트 가져오기.
- 새 기능·수정 절차는 [기능 추가·수정 필수 체크리스트](FEATURE_CHANGE_CHECKLIST.md)를 따른다. 진입점이 다른 기능의
  UI라면 설치기 `Requires`, 모듈 `depends`, DLL AND를 같은 방향으로 맞춘다.
- 새 모드 절차(현재): 매니페스트 + `installer_map.json` 항목 추가 → `gen_installer_catalog --apply` →
  `gen_module_manifests --apply` → `mod_compat`/`installer_preflight` → 중앙 등록부 반영 → `gen_module_table`.
- 검증: installer GUI 556(레지스트리 530)·기능 스위치 77·왕복 822검사, 카탈로그/매니페스트 0문제, preflight 0문제, `dll-split -Build` 통과. 런타임 소스·DLL 코드는 변경하지 않았다.

## 12. 3단계 후보

- `features.json`/`module_registry.json`을 매니페스트에서 생성(현재는 거울 + 손 관리).
- 다른 모더 모드의 설치 패키지에서 매니페스트를 추출하는 가져오기 도구(바이너리 실행 없이 파일·문자열 근거만).
- 새 기능 설명을 추가할 때 `installer_map.json`의 `note`를 갱신하고 `gen_installer_catalog --apply`를 실행한다.
