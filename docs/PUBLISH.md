# GitHub 공개 배포 절차

사용자 지시(2026-11-01): 공개 배포 절차를 한곳에 둔다. 저장소 `t3ferii/san14ce-ai-mod`. 공개는 사용자 요청이 있을 때만 한다. 배포 지시가 공개 승인이므로 다시 묻지 않고, 사전 릴리스로 올리지 않는다. 2026-11-16 정리: 판별 보완 절을 아래 단계에 합쳤다(마지막 실제 배포 v16.3.1 기준).

## 원칙
- 커밋 작성자·커미터는 `t3ferii <188655834+t3ferii@users.noreply.github.com>`만. 실명·gmail 금지. 새 복제본마다 저장소 로컬 `user.name`/`user.email`을 먼저 설정한다.
- 토큰을 직접 다루지 않는다. push는 Git Credential Manager(`credential.helper=manager`)가, 릴리스 생성은 임시 브랜치의 GitHub Actions(`github.token`)가 한다. `gh` CLI는 이 PC에 없다.
- 배포는 **미서명**이라고 적는다.
- 배포 ZIP(`SAN14_AI_<버전>_integrated.zip`)에는 설치 관리자 EXE(`AI모드_설치관리자.exe`)와 DLL 6개(`dinput8`, `SAN14_Layout`, `SAN14_CombatAI`, `SAN14_Convenience`, `SAN14_CCZ`, `SAN14_ModPack`)가 들어간다. `.cmd`·`.bat`는 넣지 않는다(2026-11-09). `_noexe.zip`은 만들지 않는다(2026-11-01). `build_share.py`가 이 구성을 검사한다.
- 릴리스마다 태그 `SAN14_AI_<버전>`과 자산 두 개를 둔다: ZIP(설치 관리자가 GitHub의 SHA-256 digest로 검사)과 `release_features.json`(변경 기능 색 표시). 새 판은 새 태그·새 릴리스로 낸다. 같은 판의 자산 교체만 태그 이동 + 자산 삭제·재업로드로 한다.
- 공개하지 않은 판(예: v15.5.1)은 `PUBLISHED`에 넣지 않고, 다음 공개판 노트에 "변경 포함"으로 적는다.
- 로컬 설치는 게임 종료 후 `install_local.py`로만 한다. 공개 패키지 준비는 실행 중인 게임을 건드리지 않는다. 공개 무로그판과 로컬 진단판의 검증 결과를 구분해 보고한다.

## 1. 빌드·패키지 (AI_TacticsMod에서, 게임 종료 상태)
```
powershell -File tools/check_feature.ps1 -Feature dll-split -Build     # 필수 분할 검사 + 공개(무로그) 빌드
powershell -File tools/build_installer.ps1                              # 설치기 + 카탈로그·화면 검사
python -B -X utf8 tools/prepare_public_release.py <버전> --layout build/input_package
python -B -X utf8 tools/build_share.py --payload build/public_<버전_밑줄>_package --report build/share_package_<버전>.json
```
- 작업 트리(`AI_TacticsMod_wt-rules`)에서는 `build/input_package`가 본 폴더로 가는 연결(junction)이라 `prepare_public_release.py`가 경로 검사에서 멈춘다. 작업 트리 안의 실제 폴더(예: `build/layout_v15_11`, 같은 bank 해시)를 `--layout`에 넘긴다.
- 결과: `dist/SAN14_AI_<버전>_integrated.zip`, `build/public_<버전_밑줄>_package/release_features.json`.
- 배포 전에 `tools/release_features.py`의 `PUBLISHED`에 직전 공개 버전을 추가한다(그 버전의 dist ZIP이 있어야 한다).
- 문서: `docs/RELEASE_NOTES.md` 맨 위에 새 판(이전 판은 「이전 배포 기록」 아래로), `docs/CHANGES_ALL.md` 표 맨 위 행, 필요하면 `README.md`(배너·다운로드 링크·항목 수). 출처 줄은 `docs/CREDITS.md`·README·`distribution/읽어주세요.txt`가 같아야 `build_share.py`가 통과한다.
- 선택: 임시 게임 폴더(EXE·`0002/ScreenLayout.rdb` 복사)에서 이전 판 → 새 판 업데이트와 해제 경로를 확인한다.

### 1-1. 배포판에는 진단·측정 코드가 없어야 한다 (사용자 2026-11-07 재확인)
- 공개 DLL은 반드시 `-RuntimeDiagnostics`/`-RuntimeCosts` **없이** 빌드한다(`check_feature.ps1 -Feature dll-split -Build`의 공개 빌드). 이때 `/DNO_RUNTIME_LOG`가 붙고 `RUNTIME_COST_PROFILE`이 없어서 `log_event` 진단, `COST_SCOPE`/`COST_CRUMB` 측정 래퍼, `#ifndef NO_RUNTIME_LOG` 블록(덤프·probe·perf)이 모두 빠진다.
- 로컬 진단판(install_local `--diagnostics-on`)의 DLL을 dist·공유 ZIP에 섞지 않는다. `build_share.py`의 진단 문자열(`day=`) 검사가 실패하면 배포를 멈춘다.
- 새 기능의 진단 코드는 `#ifndef NO_RUNTIME_LOG` 또는 `diagnostic_logging` 아래에만 둔다. 공개 빌드에 남는 로그 호출이 있으면 배포 전에 고친다.

### 1-2. UI 자료만 바뀌는 배포
- 사용자 요청으로 DLL을 유지하면 네이티브 소스·DLL 해시가 같은지 확인하고 설치기·리소스 검사만 실행한다. DLL 재빌드나 전체 회귀를 했다고 보고하지 않는다.
- [UI 리소스 복원 기준](UI_RESOURCE_LIFECYCLE.md)에 따라 소유 리소스·원본 참조·OFF/해제를 검증한다.

### 1-3. README 스크린샷 (`readme_screenshot/`)
- 사용자가 새 기능 사진·영상을 `AI_TacticsMod/readme_screenshot/`에 넣는다. 배포 때 저장소 `readme_screenshot/`에 그대로 복사해 커밋한다(파일 이름 유지).
- README의 해당 기능 절에 `![설명](https://raw.githubusercontent.com/t3ferii/san14ce-ai-mod/main/readme_screenshot/<URL 인코딩한 파일 이름>)` 줄을 넣는다. 공백·괄호·한글은 퍼센트 인코딩한다.
- 배포 전에 `readme_screenshot/`에서 README에 아직 안 쓰인 파일을 찾아 모두 해당 절에 넣는다. 파일 이름의 안내(예: 「gif나 webp 둘중하나 쓰시오」)는 따른다.
- 저장소에 올리는 것은 이 폴더의 사진·영상뿐이다. 로컬의 다른 이미지나 개인 경로는 올리지 않는다.

## 2. main 문서 갱신
```
git clone https://github.com/t3ferii/san14ce-ai-mod.git repo   # 스크래치 폴더에
git config user.name t3ferii; git config user.email 188655834+t3ferii@users.noreply.github.com
```
- 저장소 README에는 로컬에 없는 「모더용 소스 공개」 절(3~9행)이 있다. 로컬 README를 통째로 덮지 말고 이 절을 유지한다.
- 소스 스냅샷을 갱신하지 않는 판에서는 README의 상대 링크(`docs/…`)가 저장소에 있는지 확인한다. 없는 문서를 가리키는 링크는 저장소 README에서만 뺀다(v16.6: `MULTI_DUEL.md`(은퇴), 다음 소스 공개 때 다시 붙는다).
- 저장소 `development/public_source_manifest.json`은 README·CHANGES_ALL·RELEASE_NOTES의 해시도 담는다. 문서만 올리는 판도 바뀐 문서의 해시를 스테이징 blob으로 고쳐 같은 커밋에 넣는다(v16.6).
- `README.md`, `docs/CHANGES_ALL.md`, `docs/CREDITS.md`, `docs/RELEASE_NOTES.md`를 LF로 옮겨 `<버전>: 요약` 커밋 후 `git push origin main`. 소스 공개를 함께 하면 같은 커밋에 넣는다(v16.2~v16.3.1 방식).
- 소스 스냅샷은 사용자 요청 시에만 갱신한다(마지막: v16.5 소스, 2026-11-20).

### 2-1. 소스 공개 갱신 (요청 시)
```
python -B -X utf8 tools/export_source_snapshot.py <스크래치>/src_<버전> "<버전> build source ..."   # 개인 경로 <user> 치환·C++ 토큰 동일 검사
python -B -X utf8 <스크래치>/src_<버전>/AI_TacticsMod/tools/audit_current_source.py                 # 스냅샷 안에서 함수 색인 재생성(로컬 development 불변)
```
- 내보내기 범위: git으로 추적하는 텍스트 파일 전부. 빠지는 것은 저장소 전용 `.github/`·`site/`·`.gitignore`·`.gitattributes`, `development/CURRENT.md`, 참고 모드 목록 `development/reference_inventory.json`, 사용자 요청 기록 `development/USER_REQUESTS.md`(사용자 말 원문, 2026-11-16 제외), `.bak` 백업이다. `assets/ccz`는 README·JSON 2개만 들어가고 `.bin`·그림·`data/`는 빠진다. 바뀌지 않은 파일은 BOM·줄바꿈을 그대로 둔다.
- 결과를 저장소에 `cp -r` 한 뒤 **README.md는 `git checkout README.md`로 되돌린다**(내보내기가 로컬 README로 「모더용 소스 공개」 절을 지움).
- `FUNCTION_AUDIT.md` 3행·`source_audit.json` basis를 새 버전으로 고친다(생성기 문구가 v12.19로 남아 있음). 내보내기의 AI-CONTRACT 주석 앵커는 `GA()` 주소표 이후의 문자열에 맞춘다. 앵커가 안 맞으면 내보내기가 멈춘다.
- 저장소 전용 파일은 유지한다: `tools/build_public_smoke.ps1`(로컬에 없음, `-Combat -Ccz`로 실행해 통과 확인), `docs/SOURCE_*.md`, `AI_EDITING.md`, `CONTRIBUTING.md`, `THIRD_PARTY.md`, `development/CURRENT.md`, `site/`.
- **빌드에 없는 옛 파일 정리 (사용자 2026-11-20)**: 복사는 덮어쓰기만 하므로 지운 파일이 저장소에 남는다. 커밋 전에 저장소 `git ls-files` 중 manifest에도, 로컬 git에도, 위 저장소 전용 목록(`LICENSE`, `readme_screenshot/`, `.github/`, `.gitignore`, manifest 포함)에도 없는 파일을 `git rm` 한다. 남는 저장소 전용 문서가 지운 파일을 가리키면 같은 커밋에서 고치고, `build_public_smoke.ps1 -Combat -Ccz`를 다시 돌린다(v16.5 뒤 78개 정리, e50d9e5).
- 저장소 `development/public_source_manifest.json`은 내보낸 파일마다 스테이징된 blob의 SHA-256으로 다시 만든다(v16.1: 687개).
- **다른 모더용 호환 지침 (사용자 2026-11-16, 다음 소스 공개부터)**: `docs/MODDER_COMPAT.md`(다른 모더와 그 AI가 함께 쓰기·고치기 전에 읽는 문서)와 `docs/FEATURE_RULES.md`는 git 추적 파일이라 자동으로 내보내진다. 내보낸 결과에 두 파일이 있는지 확인하고, 저장소 전용 파일 세 곳을 같이 고친다.
  - README 「모더용 소스 공개」 절의 링크 줄 맨 앞에 「다른 모더용 호환 지침」 링크(대상 docs/MODDER_COMPAT.md)를 넣는다. 본문의 공개 버전·날짜도 이번 판으로 고친다.
  - 저장소 전용 `AI_EDITING.md` 첫 문단 앞에 「작업 전에 다른 모더용 호환 지침(MODDER_COMPAT.md)과 새 기능 규칙(FEATURE_RULES.md)을 읽는다.」를 두 문서 링크와 함께 넣는다.
  - `CONTRIBUTING.md`의 「AI에 맡길 때는 AGENTS.md와 docs/AI_EDITING.md를 읽도록 하고」를 「AI에 맡길 때는 docs/MODDER_COMPAT.md, AGENTS.md, docs/AI_EDITING.md를 읽도록 하고」로 바꾼다.
  - 내보낸 `AGENTS.md` 첫 항목이 외부 작업자를 MODDER_COMPAT으로 보내는지 확인한다(로컬 원본에 있음).
- 커밋 전 `git diff --cached`에서 실명·gmail·`C:\Users\<user> 경로가 없는지 확인한다.

## 3. 임시 브랜치로 릴리스 생성
main 커밋 해시를 `MAIN`으로 기억한 뒤:
```
git checkout -b release/<버전>
mkdir release_assets   # ZIP, release_features.json, notes.md(RELEASE_NOTES의 새 판 본문, 제목줄 제외)
```
- `release_assets/.gitattributes`에 `* -text`를 넣는다. 없으면 Windows `core.autocrlf`가 `release_features.json`의 줄바꿈을 바꿔 해시가 어긋난다.
- **ZIP은 `git add -f`로 넣는다.** 저장소 `.gitignore`가 `*.zip`을 무시한다.

`.github/workflows/release-<버전>.yml` (v16.3.1에서 성공한 형태: 초안 생성 → 자산 2개 업로드 → 크기·SHA-256 digest 대조 → 공개):
```yaml
name: Release <버전>
on:
  push:
    branches: [release/<버전>]
permissions:
  contents: write
jobs:
  release:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Create release
        env:
          TOKEN: ${{ github.token }}
          REPO: ${{ github.repository }}
          TAG: SAN14_AI_<버전>
          TARGET: <MAIN 커밋 해시, 전체 40자>
          TITLE: "<버전> · 삼14CE AI교정 모드"
        run: |
          python3 - <<'PY'
          import hashlib, json, os, traceback, urllib.error, urllib.request
          def call(url, data=None, ctype='application/json', method=None):
              req = urllib.request.Request(url, data=data, method=method, headers={'Authorization': 'Bearer ' + os.environ['TOKEN'],
                  'Accept': 'application/vnd.github+json', 'Content-Type': ctype, 'User-Agent': 'release'})
              with urllib.request.urlopen(req, timeout=600) as r: return json.load(r)
          try:
              repo = os.environ['REPO']; api = 'https://api.github.com/repos/' + repo
              rel = None
              for r in call(api + '/releases?per_page=50'):
                  if r['tag_name'] == os.environ['TAG']: rel = r; break
              if rel is None:
                  rel = call(api + '/releases', json.dumps({'tag_name': os.environ['TAG'], 'target_commitish': os.environ['TARGET'],
                      'name': os.environ['TITLE'], 'body': open('release_assets/notes.md', encoding='utf-8').read(),
                      'draft': True, 'prerelease': False}).encode())
              have = {a['name']: a for a in rel['assets']}
              up = rel['upload_url'].split('{')[0]
              for name, ctype in (('SAN14_AI_<버전>_integrated.zip', 'application/zip'), ('release_features.json', 'application/json')):
                  data = open('release_assets/' + name, 'rb').read()
                  want = 'sha256:' + hashlib.sha256(data).hexdigest()
                  a = have.get(name)
                  if a is None:
                      a = call(up + '?name=' + name, data, ctype)
                  if a.get('size') != len(data) or a.get('digest') != want:
                      raise SystemExit('asset mismatch %s size %s/%s digest %s/%s' % (name, a.get('size'), len(data), a.get('digest'), want))
                  print('::notice::asset %s %s %s' % (name, a.get('size'), a.get('digest')))
              rel = call(api + '/releases/%d' % rel['id'], json.dumps({'draft': False, 'prerelease': False, 'make_latest': 'true'}).encode(), method='PATCH')
              print('::notice::published %s draft=%s %s' % (rel['tag_name'], rel['draft'], rel['html_url']))
          except BaseException as e:
              detail = e.read().decode()[:400] if hasattr(e, 'read') else ''
              print('::error::' + repr(e)[:300] + ' ' + ' '.join(detail.split()))
              print(traceback.format_exc())
              raise SystemExit(1)
          PY
```
커밋 후 `git push origin release/<버전>`. 릴리스 작성자는 `github-actions[bot]`으로 보인다.
- 초안은 `/releases/tags/<tag>`로 보이지 않는다. 워크플로처럼 `/releases` 목록에서 찾는다.
- `gh release create`는 러너에서 실패했다(로그 비공개라 원인 미확인). 위처럼 REST API를 python으로 부른다.
- 실행 결과 확인: `/actions/runs?branch=release/<버전>` → run id → `/actions/runs/<id>/jobs` → job id → `/check-runs/<job id>/annotations`(`::notice::`·`::error::`, 로그인 불필요).
- 자산 올리기가 실패하면 릴리스는 초안으로 남아 공개 페이지에 보이지 않는다. 고쳐서 다시 push하면 빠진 자산만 올리고 공개한다.

## 4. 확인과 정리
- `https://api.github.com/repos/t3ferii/san14ce-ai-mod/releases/tags/SAN14_AI_<버전>`에서 자산 두 개, ZIP의 `digest`가 `sha256:<로컬 ZIP 해시>`인지 확인한다.
- 설치 관리자 쪽 확인: 설치된 EXE를 PowerShell에서 `[Reflection.Assembly]::LoadFile`로 열어 `AiTacticsInstaller.Updates.Fetch()`가 새 버전과 `HistoryUrl`을 돌려주는지 본다.
- 확인 후 임시 브랜치 삭제: `git push origin --delete release/<버전>` (태그·릴리스는 main 커밋을 가리키므로 남는다).
- 로컬 기록: `docs/CHANGES_ALL.md` 상태 칸, 필요하면 `tools/generate_handoff.py`(본 폴더에서만. 작업 트리에서 돌리면 설치 기록을 못 읽어 CURRENT.md를 「미설치」로 덮는다. `local_install.ps1`은 설치 때 이미 갱신한다).
- 다음 배포 전에 `tools/release_features.py`의 `PUBLISHED`에 방금 공개한 판을 넣는다(v16.4에서 빠뜨려 v16.5 때 채움).
- 메뉴 항목 수 차이를 노트에 쓰기 전에 확인: `release_features.py`는 현재 판만 `Retired=$true` 행을 빼고, 이전 배포 ZIP은 은퇴 행까지 센다. 빠진 항목은 이전 ZIP의 `FeatureSelect.ps1`에서 보이던 행인지 본다(v16.5: 14개 중 13개가 이미 은퇴). 직전 공개 뒤 개발판에서 생겼다가 이번에 은퇴한 키는 공개 사용자가 본 적이 없으니 노트에 「빠졌다」고 쓰지 않는다(v16.6: 개발용 다중 일기토 두 항목, `git log -S <키> -- development/installer_map.json`으로 생긴 판 확인).
- 릴리스 본문만 고칠 때(이 PC에 gh 없음): 임시 브랜치 `release/<버전>-notes`에 `release_assets/notes.md`와, 태그로 릴리스를 찾아 `{"body": ...}`만 PATCH하는 워크플로를 올리고, 확인 뒤 브랜치를 지운다(v16.5).
