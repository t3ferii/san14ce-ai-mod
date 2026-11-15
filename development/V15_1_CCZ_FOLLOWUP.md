# v15.1 로컬 후속 수정 및 지형 렉 조사 (2026-11-02)

## 이번 변경

- 설치관리자는 설치 상태의 release_version과 같은 버전인 Apply 매니페스트에서 local_diagnostic_only를 읽는다. 버전이 다른 준비 패키지를 현재 설치판으로 오인하지 않는다.
- 공개 최신판이 같으면 「공개 최신판」, 로컬 진단판이면 「로컬 개발판 … · 공개 …」, 공개판이 더 새로우면 「업데이트 가능」으로 표시한다. 버전이 더 낮은 공개판은 자동 업데이트 대상이 아니다.
- [ccz] ccz_far_keep는 기본 0이고 ccz_sprites의 하위 옵션이다. 0이면 stop_distance 활성화, 오래된 카메라 갱신, far_position 기반 재수집을 모두 사용하지 않는다.
- local14의 정상 UI-depth 렌더링 및 3/8 해상도 예비 렌더링(ccz_sprites_zoom_fallback)은 그대로 유지한다. 지형 깊이·깃발·UI 경로는 변경하지 않는다.
- CCZ 모듈 ABI 2로 far_keep 값을 전달한다. 새 Convenience/CCZ DLL을 함께 사용해야 하며 구 ABI는 기존 검사로 거부된다.

## 실게임 로그 확인

진단판 및 기존 진단 로그 설정 ON에서 게임을 완전히 종료한 뒤 옵션을 바꾸어 비교한다.

- ccz_sprites_far_keep: army=선택값(0/1), value=보존 제한 600000ms.
- ccz_sprites_zoom_fallback: local14 예비 렌더링 결과. far_keep=0에서도 이 경로는 살아 있다.
- ccz_sprites_zoom_recovered: 그리기 실패 또는 마지막 제출 후 500ms 이상의 공백 뒤 실제 Draw 제출이 다시 성공한 경우. 공용 Present가 생략된 구간도 포함한다. army=제출 그림 수(라벨 포함), value=far_keep(0/1), hex=장면 경로 여부. 최대 32회 기록한다.
- 복귀 로그는 Draw 제출 근거이며 화면상 정상 표시·깃발 겹침·지형 가림의 증명은 아니다. 줌아웃→줌인 반복 후 사용자 화면 확인이 필요하다.

## A/B/C 사용자 관찰과 후속 조사

동일 조건에서 사용자 관찰: 0(foam ON/render ON) 렉, 1(OFF/ON) 렉, 2(ON/OFF) 렉, 3(OFF/OFF) 렉 없음.

이 결과는 renderer와 foam 각각 켜진 조합에서도 렉이 발생한다는 근거다. 한쪽만 원인이라고 단정하거나 두 모듈이 동시에 켜져야만 발생하는 문제로 한정하지 않는다. 공통 하위 경로가 있는지도 확인한다. C에서도 나무·편집 메뉴는 살아 있다.

1. foam 단독(B): 원작 콜백별 호출 수/총 시간/최대 시간, dirty 큐 길이, 원작 35개 효과 이동·숨김 스캔, 줌 전환의 세대 변경·락 대기를 분리 측정한다. 콜백별 설치/원작 전달만 유지한 단계와 실제 갱신 단계를 비교한다.
2. renderer 단독(A): CreateTexture2D 호출 수, 대상/비대상 분류 비용, 텍스처 교체·해제와 중복 캡처, Present 업로드 바이트·dirty 처리량, CPU/GPU 동기화 및 slot/ctrl 락 대기를 분리 측정한다. 텍스처 관찰만, 높이 캡처만, 업로드만 단계적으로 격리한다.
3. 공통 원인: 카메라 줌 이벤트와 world 전환·resync, terrain_edit 공통 통지, 공유 Present 경로, 진단 기록 비용을 확인한다. 평균보다 느린 프레임 직전의 최대 시간과 호출 폭증을 대조한다.
4. 조건 고정: 같은 세이브·화면 해상도·카메라·CCZ 설정으로 0/1/2/3을 매번 재시작해 비교한다. ccz_far_keep는 우선 0으로 고정한다. A/B/C에서 지형을 편집하거나 저장하지 않는다.

실게임 확인은 사용자가 수행한다. 이 변경으로 렉이 해결되었다고 판정하지 않는다. GitHub 공개 작업은 없다.

## 빌드·설치·복구

- 로컬 후속판: v15.1-cczfar-local1. 미서명 진단판. 전투 DLL·입력 로더는 기존 설치본을 유지한다.
- 설정: ccz_far_keep=0 추가, 기존 terrain_edit=1 및 terrain_diag_mode=3을 포함한 사용자 값을 보존한다.
- 설치기 정적 검사 451개 및 설정 왕복/업데이트 상태 fixture 580개 통과.
- CCZ 정적/원작 바이트 검사 71개 통과. local14 set_targets 함수 전체 LF 정규화 SHA-256을 고정해 정상/예비 경로 보존을 확인한다.
- 필수 check_feature.ps1 -Feature dll-split -Build 통과(core의 native/merit/recruitment 검사 포함). 이후 복귀 로그의 Present 생략 대응만 보완하고 CCZ 검사 및 진단 DLL·설치기 빌드를 재실행해 통과했다.
- 진단 export: Convenience=AIInitialize/SAN14ConvenienceInitialize, CCZ=SAN14CczGetApi. CCZ GetApi(1) 거부 및 GetApi(2) 승인 확인. 게임 함수는 실행하지 않았다.
- 검사 기록: build/development/v15_1_far_keep_split_check.log, v15_1_far_keep_diagnostic_build.log, v15_1_far_keep_installer_check.log.
- 설치 증명: build/v15_1-cczfar-local1_installation.json. 사용자 설정·원본 EXE·미선택 모듈 보존 및 설치본/Apply/매니페스트 해시 일치 여부를 기록한다.
- 이전 v15.1로 복구: 게임 종료 후 게임 루트에서 `python -B -X utf8 AI_TacticsMod/tools/install_local.py rollback v15.1-cczfar-local1`. 그 뒤 `python -X utf8 AI_TacticsMod/tools/generate_handoff.py`.
- 기존 SAN14_AI_v15.1_integrated.zip은 이전 후보를 그대로 보존한다. 이번 후속 수정은 그 ZIP에 포함되어 있지 않다.

변경 소스: installer/InstallerGui.cs, distribution/FeatureSelect.ps1, FeatureSelect.ps1,
src/native_adapter.cpp, src/ccz_module_api.h, src/ccz_module.cpp, src/native_ccz_bridge.inl,
src/native_split_install.inl, src/native_ccz_sprites.inl,
tests/verify_ccz_sprites.py, tests/installer_gui_roundtrip.ps1.

최종 설치 결과: installed_and_verified. 기존 사용자 설정 전부 보존, ccz_far_keep=0,
terrain_edit=1/terrain_diag_mode=3 보존, 소스 스냅샷 일치, 설치본/Apply/매니페스트 해시 일치.
업데이트 전 백업: backup/local_install_08247527949b486290abd4244b0fafea.

| 최종 산출물 | SHA-256 |
|---|---|
| SAN14_Convenience.dll | 0d1bbb5324663e1ab7ff206572627cd35667e512adfc04ef675364e29e411196 |
| SAN14_CCZ.dll | fc7dbdbafdfd0c70fb07fdbd711ff0c9c051182b66ba05659fc8ffaad10cecb1 |
| AI모드_설치관리자.exe | 5fe81be2906c6d292dad5285d51055da9a7cbfe66125356d1cebaf72365e1b62 |
