# GitHub 공개 배포 절차

사용자 지시(2026-11-01): 매번 다시 찾지 않도록 공개 배포 절차를 한곳에 둔다. v15 배포에 실제로 쓴 순서다. 저장소 `t3ferii/san14ce-ai-mod`. 공개 전에 사용자 확인을 받는다(외부 공개).

## 원칙
- 커밋 작성자·커미터는 `t3ferii <188655834+t3ferii@users.noreply.github.com>`만. 실명·gmail 금지. 새 복제본마다 저장소 로컬 `user.name`/`user.email`을 먼저 설정한다.
- 토큰을 직접 다루지 않는다. push는 Git Credential Manager(`credential.helper=manager`)가 처리하고, 릴리스 생성은 임시 브랜치의 GitHub Actions(`github.token`)가 한다. `gh` CLI는 이 PC에 없다.
- 배포는 **미서명**이라고 적는다. `_noexe.zip`은 만들지 않는다(사용자 2026-11-01).
- 설치 관리자 업데이트 기능(v15~)은 릴리스마다 `SAN14_AI_<버전>` 태그, `*_integrated.zip`(GitHub가 계산하는 SHA-256 digest로 검사), `release_features.json`(변경 기능 색 표시) 두 자산을 요구한다.

## 1. 빌드·패키지 (AI_TacticsMod에서, 게임 종료 상태)
```
powershell -File tools/check_feature.ps1 -Feature dll-split -Build     # 필수 분할 검사 + 공개(무로그) 빌드
powershell -File tools/build_installer.ps1                              # 설치기 + 카탈로그·화면 검사
python -B -X utf8 tools/prepare_public_release.py <버전> --layout build/input_package
python -B -X utf8 tools/build_share.py --payload build/public_<버전_밑줄>_package --report build/share_package_<버전>.json
```
- 결과: `dist/SAN14_AI_<버전>_integrated.zip`, `build/public_<버전_밑줄>_package/release_features.json`.
- 다음 배포 전에 `tools/release_features.py`의 `PUBLISHED`에 직전 공개 버전을 추가한다(그 버전의 dist ZIP이 있어야 한다).
- 문서: `docs/RELEASE_NOTES.md` 맨 위에 새 판(이전 판은 「이전 배포 기록」 아래로), `docs/CHANGES_ALL.md` 표 맨 위 행, 필요하면 `README.md`(배너·다운로드 링크·항목 수). 출처 줄은 `docs/CREDITS.md`·README·`distribution/읽어주세요.txt`가 같아야 build_share가 통과한다.
- 선택: 임시 게임 폴더(EXE·`0002/ScreenLayout.rdb` 복사)에서 이전 판 → 새 판 → Restore → 이전 판 Apply로 관리.ps1 업데이트·되돌리기 경로를 확인한다.

## 2. main 문서 갱신
```
git clone https://github.com/t3ferii/san14ce-ai-mod.git repo   # 스크래치 폴더에
git config user.name t3ferii; git config user.email 188655834+t3ferii@users.noreply.github.com
```
- 저장소 README에는 로컬에 없는 「모더용 소스 공개」 절(3~9행)이 있다. 로컬 README를 통째로 덮지 말고 이 절을 유지한다.
- `README.md`, `docs/CHANGES_ALL.md`, `docs/CREDITS.md`, `docs/RELEASE_NOTES.md`를 LF로 옮겨 `docs: <버전> — 요약` 커밋 후 `git push origin main`.
- 소스 스냅샷은 사용자 요청 시에만 갱신한다(마지막: v15, main `d7f1d58`, 2026-11-01).

## 2-1. 소스 공개 갱신 (요청 시)
```
python -B -X utf8 tools/export_source_snapshot.py <스크래치>/src_<버전> "<버전> build source ..."   # 개인 경로 <user> 치환·C++ 토큰 동일 검사
python -B -X utf8 <스크래치>/src_<버전>/AI_TacticsMod/tools/audit_current_source.py                 # 스냅샷 안에서 함수 색인 재생성(로컬 development 불변)
```
- 결과를 저장소에 `cp -r` 한 뒤 **README.md는 `git checkout README.md`로 되돌린다**(내보내기가 로컬 README로 「모더용 소스 공개」 절을 지움).
- `FUNCTION_AUDIT.md` 3행·`source_audit.json` basis를 새 버전으로 고친다(생성기 문구가 v12.19로 남아 있음).
- 저장소 전용 파일 유지: `tools/build_public_smoke.ps1`(로컬에 없음, `-Combat -Ccz` 실행해 통과 확인), `docs/SOURCE_*.md`, `AI_EDITING.md`, `development/CURRENT.md`, `public_source_manifest.json`(추가·변경 파일 지문), `THIRD_PARTY.md`, `site/`.
- 커밋 전 `git diff --cached`에서 실명·gmail·`C:\Users\<user> 경로가 없는지 확인한다.

## 3. 임시 브랜치로 릴리스 생성
main 커밋 해시를 `MAIN`으로 기억한 뒤:
```
git checkout -b release/<버전>
mkdir release_assets   # ZIP, release_features.json, notes.md(RELEASE_NOTES의 새 판 본문, 제목줄 제외)
```
`.github/workflows/release-<버전>.yml` (v15에서 성공한 형태. 릴리스가 없으면 만들고, 빠진 자산만 올린다. 실패하면 오류를 `::error::` 주석으로 남겨 로그인 없이 `/check-runs/<job id>/annotations` API로 읽을 수 있다):
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
          TARGET: <MAIN 커밋 해시>
          TITLE: "<버전> · 삼14CE AI교정 모드"
        run: |
          python3 - <<'PY'
          import json, os, traceback, urllib.error, urllib.request
          def call(url, data=None, ctype='application/json'):
              req = urllib.request.Request(url, data=data, headers={'Authorization': 'Bearer ' + os.environ['TOKEN'],
                  'Accept': 'application/vnd.github+json', 'Content-Type': ctype, 'User-Agent': 'release'})
              with urllib.request.urlopen(req, timeout=300) as r: return json.load(r)
          try:
              repo = os.environ['REPO']
              try:
                  rel = call('https://api.github.com/repos/%s/releases/tags/%s' % (repo, os.environ['TAG']))
              except urllib.error.HTTPError as e:
                  if e.code != 404: raise
                  rel = call('https://api.github.com/repos/%s/releases' % repo, json.dumps({'tag_name': os.environ['TAG'], 'target_commitish': os.environ['TARGET'],
                      'name': os.environ['TITLE'], 'body': open('release_assets/notes.md', encoding='utf-8').read(), 'make_latest': 'true'}).encode())
              have = {a['name'] for a in rel['assets']}
              up = rel['upload_url'].split('{')[0]
              for name, ctype in (('SAN14_AI_<버전>_integrated.zip', 'application/zip'), ('release_features.json', 'application/json')):
                  if name in have: continue
                  a = call(up + '?name=' + name, open('release_assets/' + name, 'rb').read(), ctype)
                  print('::notice::uploaded %s %s %s' % (name, a.get('size'), a.get('digest')))
          except Exception as e:
              detail = e.read().decode()[:400] if hasattr(e, 'read') else ''
              print('::error::' + repr(e)[:300] + ' ' + ' '.join(detail.split()))
              print(traceback.format_exc())
              raise SystemExit(1)
          PY
```
- **ZIP은 `git add -f`로 넣는다.** 저장소 `.gitignore`가 `*.zip`을 무시한다(v15 첫 시도에서 자산이 빠짐).
- `gh release create`는 러너에서 실패했다(로그 비공개라 원인 미확인). 위처럼 REST API를 python으로 부른다.
- 실행 결과 확인: `/actions/runs?branch=release/<버전>` → run id → `/actions/runs/<id>/jobs` → job id → annotations.
- 릴리스가 먼저 생기고 자산 올리기만 실패하면 공개 페이지가 빈 릴리스로 보인다. 바로 고쳐서 다시 push한다.

커밋 후 `git push origin release/<버전>`. 릴리스 작성자는 `github-actions[bot]`으로 보인다.

## 4. 확인과 정리
- `https://api.github.com/repos/t3ferii/san14ce-ai-mod/releases/tags/SAN14_AI_<버전>`에서 자산 두 개, ZIP의 `digest`가 `sha256:<로컬 ZIP 해시>`인지 확인한다.
- 설치 관리자 쪽 확인: 설치된 EXE를 PowerShell에서 `[Reflection.Assembly]::LoadFile`로 열어 `AiTacticsInstaller.Updates.Fetch()`가 새 버전과 `HistoryUrl`을 돌려주는지 본다.
- 확인 후 임시 브랜치 삭제: `git push origin --delete release/<버전>` (태그·릴리스는 main 커밋을 가리키므로 남는다).
- 로컬 기록: `docs/CHANGES_ALL.md` 상태 칸, 필요하면 `tools/generate_handoff.py`.
