# 전체 빌드에 필요한 미포함 자료

이 저장소만으로 실행 가능한 검사는 `tools/build_public_smoke.ps1`이다. `-Combat` 옵션은 외부 payload 없이 실제 전투 DLL도 빌드한다. 다음 자료를 갖추기 전에는 두 모듈 전체 split 재빌드·원작 검사·설치가 완료된다고 주장하지 않는다. 누락 자료를 guard 삭제나 임의 바이트로 대체하지 않는다.

| 입력·경로 (저장소 기준, `../`는 게임 폴더) | 소비 경로·의미 |
|---|---|
| `../SAN14CE_KO.exe` | 원작 파일·PE/바이트 검사. 해시 정책은 `native_profile.h`와 각 검사 도구를 확인한다. 다른 빌드는 개별 ABI/호출자/바이트 재검증 필요 |
| `legacy/FogMod_Source/native_game_analysis/running_code_text.bin` | 해당 지원판의 실제 코드 dump. 원시 게임 코드는 공개하지 않는다. `build_split -Fixture`와 여러 원작 범위 검사 입력. 디스크 EXE와 동일하다고 가정해 임의 생성하지 않는다 |
| `../reference_mod/Editor_Source/san14ce/{traits9ui,traits9abil,traits9list,codepatch,senpotex,traitlayout}.py` | `prepare_traits9.py`가 선택한 정의를 읽어 manifest·9개성 UI/화면 자료 생성. 원본 에디터 출처·권한은 별도 확인 |
| `research/modpack_v16/extracted/payload/SAN14_TraitHover.dll` | `import_modpack_objects.py`의 hover COFF 입력. SHA-256 `8cfad1a6ada78ecab898bef9d166167874d730a757a94d3f707b60cca1ef91bc` |
| `research/modpack_v16/extracted/payload/SAN14_BattleDeaths.dll` | 같은 도구의 deaths COFF 입력. SHA-256 `1f354aec9b1e2ee29d0f07e552e32f87606ca1bf8e21738c0db631cff56cf696` |
| `../0002/ScreenLayout.rdb`와 참조 bank | `prepare_traits9.py`의 원작 index/layout 입력. 원작이 수정된 상태면 기존 백업·설치 상태에 따른 역병합 자료도 필요 |
| `backup/input_state.json`, `build/input_package/*`, 관련 `research/.../layout_patch` | 기존 설치를 갱신할 때만 필요한 상태·패키지. 개인 설치 이력을 배포하지 않는다. 과거 설치 상태를 새 checkout에 임의로 만들어 넣지 않는다 |
| 관련 `research/` 정적 근거·검사 JSON·원작 캡처 | 일부 기능별 verify 스크립트는 파일로 저장된 조사 근거를 추가 입력으로 요구한다. 이 목록은 기본 split 빌드의 주요 입력이며 모든 역사적 검사 자료를 망라하지 않는다 |

표의 외부 소스/DLL은 이 저장소에 들어 있지 않다. 바이너리에서 만든 `.obj`, `modpack_payloads.h`, `traits9_payloads.h`, 게임 layout 추출물도 포함하지 않는다. 공개 C++는 외부 코드에 대한 wrapper와 hook 연결을 포함하므로 외부 구현까지 모두 독자 C++ 소스라는 뜻은 아니다.

Python PE 파서 `legacy/FogMod_Source/tools/audit_native_game.py`는 도구들이 import하는 독자 로컬 helper라 포함했다. 파일을 읽어 PE를 해석할 뿐, 원본 바이너리 자체는 포함하지 않는다. capstone은 별도 Python 의존성이다. 원래 개인 PC의 capstone 검색 경로 두 곳은 제거했으며 현재 Python 환경에 설치하여 사용한다.
