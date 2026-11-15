# U35 인계 — local29 설치 완료 (2026-11-10)

## 작업 범위와 제한
- 위치: `AI_TacticsMod_wt-rules`, 브랜치 `v1571-plus`. 먼저 메인 `AI_TacticsMod/docs/SESSION_RULES.md`를 읽는다.
- 목표: 진행 버튼부터 플레이어 전략턴까지 렉 제거. 인원 제한·원작 규칙 변경으로 우회하지 않는다.
- 한국어로 보고. **미서명** 표기. GitHub 게시는 사용자 지시 때만.
- 원작 호출은 원래 호출 스레드에서만. Computer Use·외부 게임 메모리 쓰기·원격 호출·원작 EXE의 REA 금지.
- 원작 근거는 `San14Sim/docs/orig/README.md`부터, 없으면 기존 Ghidra 추출본 조회.
- RTK·Serena 삭제됨. 재사용/재설치하지 않는다. 서브에이전트 금지.
- **사용자 최신 지시: 설치 후 인계만 하고 종료. 토큰 절약. 추가 측정·빌드·검증 문서 생성 금지. 다음 작업은 새 지시 후 진행.**

## 설치 상태
- `local29` 설치 성공. 실제 코드 커밋 `6993e9e`.
- 공용 `install_local.py prepare/install` 사용. 편의·Layout DLL과 설치기만 교체.
- 다른 DLL 3개, 설정 모든 값, 메인 보호문서 보존. 임시 복사한 소스 3개 원복 완료.
- 설치 증빙: 메인 `build/local29_installation.json` (`installed_and_verified`). 실제 워크트리 소스 해시는 워크트리 `build/development/u35_local29_main_files/metadata.json`에 있음. 공용 설치 증빙의 메인 소스 목록과 구분할 것.
- local29 **실게임 효과 미확인**. 다음 CPU 기록은 시작하지 않았음.

## 구현 및 실측
- local27: 870470 래퍼 경로의 레이아웃 보관 복구.
- local28: 첫 느린 동기 로드(5ms)부터 보관. 최대512개, 실패 예약 취소/재시도. 기존 `performance.layout_keep` 사용.
- local28b 기록 성공: PID41232, 손실0. HUD8개에서 두 번째20~32ms 로드 소실,136개 보관.
- 같은 게임 날짜80490→80550/각50초, 주요2스레드 레이아웃86E920 CPU표본: local26 2205ms → local27b 799ms → local28b 619ms.
- 프로세스 전체 CPU 창 합계: local27b 약119031ms → local28b 약119844ms. **전체 CPU 개선은 확인 못 함.** 멀티스레드 CPU 합계이며 벽시계 멈춤 시간이 아님.
- 기존499/527/526ms는 시작 로딩 포함 고부하1초 구간 평균이다. 날짜1회 멈춤으로 해석하지 말 것.
- local28b 메모리: PrivateBytes 표본최대7.01GiB, 작업집합최대2.90GiB. 27b 동일최대값 없어 증가량 비교 불가.
- local29: 남은 UI 이미지 대기의 주경로85AE70(kind10 작은얼굴)를 캐시에 연결. CALL892234/670AF1/670B53/670BA2 네 곳을 Layout owner `face_keep_portrait`로 관리. 원작/기존next 실행 후 이미 로드된 얼굴에만 참조 추가. **기존256개 LRU·메모리압박 정책을 공유**, 새 풀 없음. 기존 `performance.face_keep` 스위치. R1폴더 있으면 새4CALL 양보. UI 디스크 자료 변경 없음.

## 검사 경계
- 통과: 얼굴 정책79/현재EXE정적33, Layout P3 40497/정적413, 분할 회귀17경로59단계 및 배포·진단빌드.
- 설치기: 정적648/설정93/왕복870, 현재INI preflight37모듈0오류.
- 별도 `feature_deps --check`는 **기존14오류 미해결**: 자동명령→군사보고, 논공→ui-common 의존선 및 기존 직접패치 카운터. 변경 전HEAD 스냅샷에서도 동일14개 재현. 검사 기준 완화 없음. 배포 전 해결 필요.
- 로그는 워크트리 `build/development/u35_local29_*`. 반복 재실행하지 말고 필요한 실패 부분만 확인.

## 다음 작업 (새 지시 후)
1. local29를 같은 세이브로 실행해 얼굴 재로딩·미세 끊김·메모리 확인. 기록할 경우 게임 실행 전에 관리자 `tools/cpu_trace.ps1 -Tag local29b`, 상태 recording 확인 후 안내. 종료 후 saved+pid 확인.
2. 얼굴 단계 확인 뒤 원작 날짜44DEE0·AI 이동비용8F5A0·부대 이동의 계산 재사용 조사. 원작 결과 대조 경로 필수. 아직 구현하지 않음.
3. 전체 렉 제거/장기 안정성/배포 준비는 미완료. 배포 ZIP은 설치기 EXE만, release_features.json 별도. 출처 표기. 게시 금지(지시 전).

## 자료 및 주의
- 분석 JSON: `research/u35_perf/scope_l26.json`, `scope_l27b.json`, `scope_l28b.json`, `layout_l28b.json`, `compare_l28b.json`. ETL은 메인 `data/performance`.
- 맵은 설치 DLL 해시에 맞게 선택. `data/performance/local27b_symbols`, `local28b_symbols` 보존. local28b의 전투 DLL은 local27 유지본이므로 재빌드된 최신 맵을 쓰지 말 것.
- `tools/cpu_scope_report.py --maps <맞는맵폴더>`로 진행 구간을 명시해 분석. CPU 표본과 실제 프레임 지연을 구분.
- 8ACF20은 공용 이미지 로더. kind10 얼굴 외 kind21 전법 그림도 포함. 프레임 훅 inclusive 시간에는 원작 호출이 포함되므로 모드 자체 비용으로 단정하지 말 것.
- INI는 메인 `AI_TacticsMod/기능설정.ini` UTF-16 LE. 설치관리자 적용 시 진단값이 꺼질 수 있음.
- 메인 README/CURRENT/CHANGES_ALL/EXTENSIONS/FEATURE_SWITCHES/docs README/읽어주세요/루트 SplitManage는 다른 세션 변경이므로 수정 금지. U35 요청 행만 메인 USER_REQUESTS에서 갱신.

## 2026-11-10 v15.10 공개 완료

사용자가 local29 실게임 속도 개선을 확인하고 공개 승인. 미서명 무로그 v15.10 공개 완료: https://github.com/t3ferii/san14ce-ai-mod/releases/tag/SAN14_AI_v15.10
소스 main 4a4ee97, C/C++ 실행 토큰 동일·공개 소스 smoke 통과. ZIP SHA256 cd1ffd01b44ce9f4d26120f56bff5687a1afd45e2b1834758fc30755cb18ae13, 17180435 bytes. GitHub 최신 정식 릴리스 및 자산 크기/해시 검증 완료. 설치된 local29 진단판은 변경하지 않음. 기존 feature_deps 14개 메타데이터 오류 미해결, 전체 CPU 감소 수치 확정 아님.

## 2026-11-10 후속 수정

- v15.10 변경 설명 누락 보완: 기존 CHANGES_SINCE_v15_7_1(local16 기준)·U24~U34·local17~29 커밋을 대조해 실제 local29 누적 목록으로 수정. 제외한 실험은 별도 명시. 공개 README/변경 이력/릴리스 본문 갱신(main fc2130a), 기존 ZIP·자산 해시 유지.
- 진단판 cost_profile이 Layout 게시 전에 내정 슬롯을 덮어 domestic_selection 두 훅이 거부되는 순서 문제 수정(20075a8). ai_trace를 모든 기능 등록 뒤로 이동하고, 측정 래퍼 설치 전에 Layout 게시. 중복 소유자 안전가드는 유지. 추적 fixture611·순서 회귀·필수 dll-split 검사/무로그 빌드·진단 빌드 통과. 실게임 수정판 검증/설치하지 않음. 현재 설치 편의 DLL은 v15.10 공개 ZIP과 해시 일치하므로 진단판으로 교체하지 않음. 미서명.
