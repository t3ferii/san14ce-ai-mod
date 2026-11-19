# 로컬 git과 병렬 작업본

사용자 지시(2026-11-06): "우리 모드도 로컬 깃 기능으로 작업을 병렬화 할수 있는건 병렬화 하자".

## 저장소
- `AI_TacticsMod/.git`은 **로컬 전용**이다. 원격(push)은 없다. 공개 저장소 배포는 [PUBLISH.md](PUBLISH.md) 절차를 따로 쓴다.
- 추적 대상은 소스뿐이다: `src/ tests/ tools/ docs/ development/ distribution/ installer/ optional_tools/ site/ .github/`와 루트의 작은 스크립트. 목록은 `.gitignore`의 허용 목록(`!/…`)이다.
- `data/ research/ legacy/ backup/ assets/ updates/ readme_screenshot/ build/ dist/`와 사용자 설정 ini는 추적하지 않는다.
- 작성자는 `t3ferii <188655834+t3ferii@users.noreply.github.com>`이다. 기준 커밋은 `c132915`이다.
- 다른 세션은 지금처럼 작업 폴더에서 바로 고쳐도 된다. git은 그 수정을 막지 않는다. 커밋은 병렬 작업본을 합칠 때만 한다.

## 병렬 작업본 (worktree)
```
powershell -File tools/worktree.ps1 -New <이름>      # ../AI_TacticsMod_wt_<이름>, 브랜치 wt/<이름>
powershell -File tools/worktree.ps1 -List
powershell -File tools/worktree.ps1 -Remove <이름>
```
- 작업본은 게임 폴더 안 `AI_TacticsMod` 옆에 만든다. 그래서 도구가 쓰는 `..`(게임 폴더)는 그대로다.
- 대용량 폴더는 원본으로 가는 junction이다. **읽기만 한다.** `build/input_package`와 `build/ui_native_analysis`도 junction이다. 나머지 `build/`와 `dist/`는 작업본 전용이다. 생성 헤더(`modpack_payloads.h`)는 복사해 둔다(`traits9_payloads.h`는 local235에 손으로 쓴 `traits9_sites.h`로 바뀌어 추적 소스다).
- 해제 스크립트는 junction을 먼저 비재귀로 지운다. junction이 하나라도 남으면 멈춘다. 공용 폴더를 지울 수 없게 하려는 장치다.
- 작업본에서는 **설치·배포·게임 실행을 하지 않는다.** 기능 검사(`check_feature.ps1 -Feature <기능>`)까지만 한다.
- 확인된 예: `trait-hover -Build`가 작업본에서 통과했다(2026-11-06).

## 합치기
1. 작업본에서 커밋한다. 메시지 끝에 `Co-Authored-By` 줄을 둔다.
2. 주 작업 폴더에서 `git status`를 본다. 그 브랜치가 고친 파일이 작업 폴더에서 수정 중(다른 세션)이면 먼저 그 세션 작업을 커밋하거나 순서를 정한다. git은 덮어쓰기 대신 병합을 거부한다.
3. `git merge --no-ff wt/<이름>` 뒤 주 작업 폴더에서 필수 분할 검사(`check_feature.ps1 -Feature dll-split -Build`)와 설치를 한다.
4. 작업본과 브랜치를 지운다.

## 나누는 기준
- 서로 다른 파일을 고치는 작업만 동시에 한다. 공용 파일(`layout_registry.h`, `gen_signatures.py`/`game_addresses_data.inl`, `features.json`, `check_feature.ps1`, `FeatureSelect.ps1`, `InstallerGui.cs`, `기능설정.ini`)은 한 작업본만 고친다. 아니면 합칠 때 주 폴더에서 한 번에 처리한다.
- 주소표 재생성(`gen_signatures.py all`)은 합친 뒤 주 폴더에서 한 번만 돌린다.
