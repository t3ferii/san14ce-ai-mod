# 11Patch 시험판 설치기 정적 구현 참고

분석일: 2026-10-15. 목적은 향후 유사 기능의 구현 참고이며 설치·적용·배포가 아니다. 설치기, 추출 DLL, 포함 Python, FFmpeg 및 제공자의 회귀 실행 파일을 실행하지 않았고 실행 중 게임 메모리에 접근하지 않았다. 추출물은 `research/reference_deepening/eleven_patch/` 안에만 저장했다.

## 대상과 근거 보존

주 대상은 `reference_mod/SAN14CE_11Patch_Installer_v1_Trial/SAN14CE_11Patch_Installer_v1_Trial/SAN14CE_11Patch_Installer.exe`이다. 이번 실제 파일은 **63,900,672 bytes**, SHA-256 **`eddb2c780d9f78ea3fc6d2ee9420e356fa7a9c65f3cf1821cb1ed60beeb45673`**다. 같은 파일명이라는 이유로 과거 7 MB 변종 또는 `(1)` 경로의 결과와 섞지 않는다.

설치기는 x64 PE32+의 .NET CIL 어셈블리다. MVID `1e219bfd-01f9-4d2b-9bb9-133533e01bfa`, metadata version `v4.0.30319`, 11 TypeDef/51 MethodDef/59 ManifestResource를 확인했다. PE 아이콘·manifest 리소스와 별개인 **CLI embedded resource 59개**에 실제 모드가 들어 있다.

| 추출된 핵심 payload | 크기 | SHA-256 |
|---|---:|---|
| DINPUT8.dll | 1,206,784 | `063417e20856f124d4ee14841aef0580445c9d6de5b07a4aadf9451afabaabdc` |
| SAN14_Traits9.dll | 7,680 | `e5ca47d3eaafd91ae50af46a3d9e631a52762730db2ded61de39c897758574c6` |
| SAN14_Traits9_UI.dll | 43,008 | `2926c3ef05abd004bf0e5d9f7113321568800a6cf4d60b7e7e0bf25367b0f164` |
| bgm_runtime.zip | 56,901,945 | `de037c774202bf17145a1da6780a542799496d2569d3e7bd29dee27220876a1e` |

전체 59개 payload의 원본 파일 offset/길이/해시, 22개 BGM ZIP 구성은 [manifest.json](../../research/reference_deepening/eleven_patch/manifest.json)에 있다. README SHA-256 `abd76c1f114e65b7d9f3b78f7e4dee270a144ff52e299d523bd47555e7b66ad7`, VERIFICATION.json SHA-256 `0b2bbcc61564c4f6379ed35dceb1d2da21fced4095cf006ccbf4fe46f723a209`도 [정적 검증 기록](../../research/reference_deepening/eleven_patch/verification_static.json)에 고정했다.

REA 근거: PE resource `ev_92599103d13c2dce483d9b0e52c2adabe119452d09c83c28844806cc1bd0cfab`; managed identity `ev_a9809534610f38b97897579cbe6310ce139151b3fd31951131c7b4ce6f0674a1`; CIL/member `ev_630ac5e80f0f62477801b574cb80f40067e66872d431b3ae4d177fb33e45d501`. [managed_members.json](../../research/reference_deepening/eleven_patch/managed_members.json)에 반환된 CIL 근거를 보존했다.

**확인 수준:** 아래의 CIL/기계어/포맷 관찰은 출하 파일의 정적 근거다. README의 기능 설명·52회귀·실행 시험 주장은 별도다. 제공 VERIFICATION 상태도 `STATIC_PACKAGE_VERIFICATION_PASSED_RUNTIME_TEST_PENDING`이다. 어떤 항목도 우리 현재 게임의 실행 검증으로 승격하지 않는다.

## 설치기의 실제 동작

재구성 C# 소스가 아니라 원본 MethodDef/CIL을 해독했다. [installer.cil.txt](../../research/reference_deepening/eleven_patch/installer.cil.txt)의 `0x0600000e ValidateTarget`, `0x06000010 ApplyDelta`, `0x06000015 Ini`, `0x0600001b RestoreManagedState`, `0x0600001c RunInstall`을 다음 조사 시작점으로 사용한다. Method token은 위 SHA/MVID에서만 유효하다.

* `ValidateTarget`는 `SAN14CE_KO.exe`, `SAN14_Launcher.exe` 존재, EXE 전체 SHA-256 `07a047826cfc316dc9e3db6bfddc72b85e8d7de99743cc73a2d7199a1ad3aa40`, 게임/런처 프로세스 부재, 상위 `appmanifest_872410.acf` 및 게임 폴더 정보를 검사한다. 기존 `DINPUT8.dll`, `version.dll`, Traits9 두 DLL, `San14ModSettings.ini`와 설치 기록의 관계도 확인한다. 분석 과정에서 이 검증 함수를 실행하지 않았다.
* 선택 항목은 0-based `bool[11]`다. `Take(9).Any(...) || on[10]`일 때 DINPUT8 및 `San14ModSettings.ini`를 쓴다. **10번 이민족/지역 데이터만 선택하면 DLL이 필요하지 않은 경로**다. 11번 개성은 Traits9와 Traits9_UI도 배치한다.
* `.san14ce11patch/backup` 및 `state.xml`에 원래 존재 여부, 원본/설치 SHA-256, backup 경로를 관리한다. `Change` 필드는 `Relative/Target/Backup/Existed/OriginalHash/InstalledHash`다. 개별 쓰기는 `AtomicWrite`, 예외 경로는 `Rollback`을 이용한다.
* `RestoreManagedState`는 복구 전 전체 파일을 점검한다. 설치 해시와 다르면 무조건 덮는 방식이 아니며, 원본 해시와 같아 Steam이 이미 복원한 상태와 설치본 변경 상태를 구분한다. 설정 파일 두 개만 `IsMutableSetting`으로 분류하며 `PreserveUserSetting`은 `.user.bak` 또는 중복 없는 `.userN.bak`에 사용자본을 보존한다.
* `SafeChild`는 저장된 상대경로를 full path로 정규화하고 root 하위인지 검사한다. ZIP 배치는 `InstallZipBundle` 경로를 통과한다. 이 구조는 설치기의 복원 설계 참고이며 우리 게임에서 rollback이 시험되었다는 의미는 아니다.

## 11개 선택 항목과 차용 범위

아래 주소는 따로 표시하지 않으면 **추출된 DINPUT8.dll 내부 RVA**다. DLL RVA를 게임 RVA로 사용하면 안 된다. unwind 범위 시작은 원본 함수명이 아니며 분리된 코드 조각일 수 있다.

| 선택 기능 | 실제 생성 key/파일 | 관리코드 또는 native 근거 | 확인 수준·남은 규칙 | 우리 배치 후보 |
|---|---|---|---|---|
| 1 인재·포로 편의 | PrisonerPersuasion, RecruitmentGate, PersonnelTooltips, TraitTooltips, CaptivePortrait, NoPositionTroops=5000/0 | `Ini` IL 0x012E~0x01C6; native CPersonnelPrisonerPreview 문자열 참조 0x32502, CPersonnelRecruitmentGate 0x32AE8; tooltip/portrait flag 표 아래 | key·native 분기/패널 준비 확인. 등용/회유 확률·비용·명령 가능 조건 전체는 미해독. NoPositionTroops의 0 처리는 별도 주의 | SAN14_Convenience.dll |
| 2 자동인사 | AutoPersonnelModule, AutoPersonnel, AutoScenery, AutoCredit | `Ini` IL 0x01CD~0x0226; 0x39D11의 INI 읽기/0x39D17 `==1`; flag 분기 0x36457, 0x439DE, 0x524D, 0x53BF | 활성 조건 확인. 무장 선발 점수·도시별 우선순위·갱신 주기는 미확인 | SAN14_Convenience.dll |
| 3 논공 동시 | DualCredit | `Ini` IL 0x022D; native flag 0xC400B; stub builder 0x25F50/0x26140, 설치 준비 0x38660 | 두 게임 분기용 trampoline 계획·on/off 검사 확인. 논공 전체 ABI/정책은 미해독 | SAN14_Convenience.dll |
| 4 문전박대 | AudienceGate=2/0 | `Ini` IL 0x0245~0x0265; native 0x39DD2~0x39DE0은 unsigned 값 0..2만 수용, 초과는 0 | 설치기 선택은 mode2. mode1/2의 정확한 방문 대상 판정은 미확인 | SAN14_Convenience.dll |
| 5 명승지·보고·연출 | Camera.AutoFocus=0/1, SkipReports/ScenicRewardOnly/ScenicNoFade/CityNoFade=1/0 | `Ini` IL 0x0094~0x0127; 실제 flag refs 아래 | 선택 시 AutoFocus가 역전되는 것과 기능별 분기 확인. 특정 보고·연출 범위는 미확인 | SAN14_Convenience.dll |
| 6 다수 부대 이동 | FormationMoveModule | `Ini` IL 0x026C; flag 0xC400E 다수 read, 0x4054에서 상태=2/대상수>=2 분기 | 여러 부대 UI 상태에 따른 실행 gate 확인. 배치 순서·간격·장애물 알고리즘 전체는 미해독 | SAN14_Convenience.dll (사용자 명령 UI) |
| 7 진행 배속 | ProgressSpeedModule | `Ini` IL 0x0284; flag 0xC400F, 0x312D9/0x4951C off 경로는 자체 시간/선택 상태 초기화 | 활성/비활성 상태 수명 확인. 배속별 배율·tick 변환은 미확인 | SAN14_Convenience.dll |
| 8 모드 설정 UI | Modules.ModSettingsUI | `Ini` IL 0x0038; native 0x33F44~0x33F63 INI 기본값1→0x39180 설치 시도 | 모듈 gate 및 16개 descriptor의 재읽기/변경 callback 구조 확인. 화면 전체 event ABI는 미확인 | SAN14_Convenience.dll |
| 9 BGM 추가·미리듣기 | Modules.BgmExpansionModule, San14BgmExpansion.ini, BGM ZIP | `RunInstall` IL 0x01AB 이후; native 0x33F88/0x37660/0x3EE40; 포함 Python 5파일 | live 소스·컨테이너·commit 규칙을 직접 분석. native 오디오 ABI 전체와 실제 재생은 미확인 | SAN14_Convenience.dll + 선택적 외부 변환 도구 |
| 10 이민족 성 일반화·지역·증축 | fix_regions 또는 결합 delta, 이벤트·메시지, 시나리오50개 delta | `RunInstall` IL 0x0285~0x0483; `delta_inventory.json` | 대상 파일·조합·exact-hash 적용 형식 확인. 게임 데이터의 도시/시설 필드 해독과 저장 소급 여부는 코드로 입증하지 못함; 새 게임 대상이라는 설명은 README 주장 | 선택적 오프라인 데이터 작업; 게임측 보조는 SAN14_Convenience.dll |
| 11 개성9·콜라보 해금 | Traits9Module, Traits9 두 DLL, fix_traits 또는 결합 delta | `RunInstall` IL 0x023A 이후/0x02C8; 아래 native 5patch 및 UI lifecycle | 5→9 코드 제한·UI hook 구조는 기계어 확인. 콜라보 해금의 세부 데이터 필드는 미해독 | SAN14_Convenience.dll + 선택적 데이터 작업 |

### native 16개 descriptor와 실제 플래그

`.rdata` RVA `0xAD2A8`, stride `0x20`, 16개 record는 UTF-16 section/key pointer, 기본 bool, 표시용 pointer를 담는다. `0x22890`의 INI 재읽기 루프는 문자열이 정확히 `1`/`0`일 때만 bool로 읽고 그 외 값은 descriptor 기본값으로 처리한다. 플래그는 `0xC4000+index`, 변경 시 `0x122D00+index*8` callback을 호출할 수 있다. 아래는 문자열 존재만이 아니라 실제 RIP 상대 플래그 참조를 확인한 결과다.

| index · section/key | native 기본값 | 확인된 참조 RVA 예 | 확인 수준 / 우리 소유 |
|---|---:|---|---|
| 0 Camera.AutoFocus | 0 | 0x22AF4 | 파생 상태로 복사 / Convenience |
| 1 Convenience.SkipReports | 1 | 0x39730 | flag 분기 / Convenience |
| 2 ScenicRewardOnly | 1 | 0x440A2, 0x45611 | flag 분기 / Convenience |
| 3 ScenicNoFade | 1 | 0x29AE6, 0x4942A | flag 분기 / Convenience |
| 4 CityNoFade | 1 | 0x34B76 | flag 분기 / Convenience |
| 5 PrisonerPersuasion | 1 | descriptor/설치기 key; 직접 RIP read 미검출 | 간접 소비 경로 미해독 / Convenience |
| 6 RecruitmentGate | 1 | descriptor/설치기 key; 직접 RIP read 미검출 | 간접 소비 경로 미해독 / Convenience |
| 7 PersonnelTooltips | 1 | 0x2989D | flag 분기 / Convenience |
| 8 TraitTooltips | 1 | 0x32DE2, 0x34462 | flag 분기 / Convenience |
| 9 CaptivePortrait | 1 | 0x205D9 | flag 분기 / Convenience |
| 10 AutoPersonnelModule | 1 | 0x36457/0x36477, 0x439DE/0x43A28, 0x44B14 | flag 읽기·제어 / Convenience |
| 11 DualCredit | 1 | 0x53DD, 0x29CC4, 0x2AB9A, 0x386C8/0x38726 | read/reset/stub에 flag 주소 전달 / Convenience |
| 12 AutoScenery | 1 | 0x524D | flag 분기 / Convenience |
| 13 AutoCredit | 1 | 0x53BF | flag 분기 / Convenience |
| 14 FormationMoveModule | 1 | 0x1254, 0x4054, 0x1C969, 0x2EE1A, 0x46B80 등 | 다수 상태 gate / Convenience |
| 15 ProgressSpeedModule | 1 | 0x26B52, 0x28E3A, 0x312D9, 0x4951C 등 | 상태 gate 및 off 초기화 / Convenience |

전체 참조 명령과 앞뒤 코드: [native_feature_refs.json](../../research/reference_deepening/eleven_patch/native_feature_refs.json). 설정 원자료: [native_config_table.json](../../research/reference_deepening/eleven_patch/native_config_table.json). 직접 RIP read가 없다는 결과는 기능이 없다는 뜻이 아니며 레지스터 기반·생성 stub·간접 callback은 이 검색 범위 밖이다.

`NoPositionTroops`는 이 bool 표가 아닌 별도 정수 parser다. DLL RVA `0x228AE`는 기본 `7000`, `0x22981` 이후는 숫자만 누적하며 마지막 유효 범위는 1000..20000이다. 설치기가 쓰는 `5000`은 유효하지만 미선택값 `0`은 이 parser에서 기본값으로 돌아간다. 따라서 **`0`을 썼다는 사실만으로 이 패널/병력 제한이 꺼진다고 가정하면 안 된다.** 상위 hook gate와 실제 화면 동작은 추가 조사 대상이다.

### 논공 동시의 조건부 분기 설계

native RVA `0x38660` 경로는 기존 설치 상태를 확인하고 가까운 실행 메모리 `0x1000` bytes를 요청한다. `0x25F50`/`0x26140`이 생성하는 코드에 DualCredit flag 주소를 넣으며, 게임 RVA `0x6C3FBF`→stub, `0x709103`→두 번째 stub의 E9 rel32 분기를 준비한다. 복귀/검증 지점으로 `0x6C3FE4`, `0x709111`, `0x709134`를 사용하고 ±2GB 분기 범위를 검사한다. 첫 builder에는 `cmp r15d,4` 및 flag byte 검사 byte sequence가 있다.

차용 가치는 실행 중 bool에 따라 원래/확장 경로를 선택하는 구조와 사전 branch-range 검증이다. 이 주소·레지스터 의미·해당 논공 분기의 ABI를 우리 빌드에서 검증한 것은 아니다. 세이브 구조 변경이나 실제 지급 결과도 여기서 입증되지 않는다.

## 개성9: 직접 확인한 5개 native patch

`SAN14_Traits9.dll`의 export `SAN14_ApplyTraits9`는 RVA `0x1570`이다. InitOnce 경로 `0x1550`→`0x1130`에서 본체 검사 후 로깅하고 UI DLL을 연결한다. 패치 record는 RVA `0x33A0`, stride `0x70`, 5개다.

| 대상 **게임 RVA** | before → after | 실제 명령 의미 |
|---|---|---|
| 0x4A6213 | `48c745dc05000000` → `48c745dc09000000` | `mov qword [rbp-0x24],5` → 9 |
| 0x4A6833 | 동일 | 별도 경로의 같은 슬롯수 5 → 9 |
| 0x224AEF | `4183fc05` → `4183fc09` | `cmp r12d,5` → 9 |
| 0x22FB7A | `83fb05` → `83fb09` | `cmp ebx,5` → 9 |
| 0x2322C7 | `4883fb05` → `4883fb09` | `cmp rbx,5` → 9 |

[traits9_patch_table.json](../../research/reference_deepening/eleven_patch/traits9_patch_table.json)은 각 원본/변경 bytes와 35~40 bytes 주변 서명까지 보존한다. 함수명이 원본에 없으므로 각 지점을 에디터/저장 검사/효과 계산 중 하나로 단정하지 않는다. 로그는 그 역할을 주장하지만 여기서 확실한 것은 다섯 개 한계값 변경이다.

* 적용 전 DOS/PE/x64/PE32+, PE timestamp **0x6A6D9F72**, SizeOfImage **0x29FF000**을 검사한다.
* 각 record의 현재 bytes가 before 또는 after인지 확인하고, patch 바깥 주변 서명도 확인한다. 혼합/충돌을 확인하지 않은 채 쓰지 않는다.
* 필요한 고유 4KB 페이지를 먼저 전부 RWX로 전환한다. 중간 실패 시 확보한 페이지의 보호를 되돌리고 bytes는 쓰지 않는다. 쓰기 후 FlushInstructionCache와 보호 복원을 한다. 이미 적용된 경우와 cache/protection 복원 경고를 별도로 반환한다.
* `Original nine-u16 save layout retained`는 내부 로그 문자열이다. 이 다섯 패치에 저장 record 확장·새 저장 파일 쓰기는 없지만 기존 저장 구조의 전체 해석까지 수행한 것은 아니다.

UI DLL export는 `SAN14_ApplyTraits9UI` RVA `0x6A90`이다. `0x1930`은 같은 build 지문 및 22개 주변 코드 서명(record stride0x48)을 검사하고, 역사/신무장 에디터 lifecycle pointer가 기대한 게임 함수인지 검사한다. 예: 게임 `0x13E9210/0x13EA128`가 `base+0x544EE0`, `0x13E9220/0x13EA138`가 `base+0x19B420`인지 확인한다. `0x16A0`에서는 최대9개 수집과 개성수>5 분기, 원작 label 생성/메서드 호출이 실제로 보인다.

확장 패널 설치 `0x3C10`, hover 설치 `0x47D0`는 각각 VirtualProtect/FlushInstructionCache/보호 복원을 수행하며 앞 단계 성공 후 뒷 단계만 실패할 수 있는 구조다. 3×3 grid, shared table/duel hover, copied trait-column descriptor라는 세부 역할은 각 루틴의 로그 및 호출 형태까지 확인했다. 모든 화면의 geometry·객체 수명·hover ABI를 완전 해독한 것은 아니다. UI label 처리 `0x16A0`, 확장 grid `0x2280`, table9 생성 `0x4B60`, descriptor 복제 `0x5240`이 다음 시작점이다.

## BGM: 포함 소스로 확인한 새 live 경로

실제 source는 [extracted/bgm_text/San14BgmExpansion](../../research/reference_deepening/eleven_patch/extracted/bgm_text/San14BgmExpansion)에 있다. 읽거나 AST로 파싱했을 뿐 import/실행하지 않았다. README·로그 문자열만으로 판단한 부분과 구분한다.

* `live_bank.py:10`은 ID **117..254**, 138개를 지원한다. `0xFF` 기본 sentinel을 보존하기 위한 상한이라는 설명은 소스 주석이며 native getter 자체는 아직 완전 해독하지 않았다. `bank.py:15`의 **117..127**은 과거 offline 확장 경로다. 파일 안의 두 한계를 하나로 합치면 안 된다. `manager.py:140`도 native_version>=2 receipt가 있으면 옛 stage_build 경로를 거부한다.
* `live_worker.py:109 prepare`는 convert/rename만 수용한다. ID 중복·범위·제목을 검사하고, 현재 live 변환은 곡 전체·trim 없음·fade 없음·320kbps를 명시한다. 일반 `bank.convert`의 기본 fade5초와 구분한다.
* `bank.py:160 convert`는 FFmpeg argv를 shell 없이 전달한다. 48kHz/stereo/s16le PCM의 정확한 sample수를 센 뒤 AAC-LC ADTS를 KTAC로 만든다. PCM 길이는 0.01초..60분이고 일반 bitrate는192/256/320이다. 이 FFmpeg를 분석 중 실행하지 않았다.
* `bank.py:106 validate_ktac`: magic `KTAC\0\0\0\1`, 전체/stream 길이, 48kHz/stereo, AAC frame 1024 samples, frame size table, `count*1024`와 delay/padding을 검증한다. 외부 input 경로를 game native 함수에 바로 넘기는 구조가 아니다.
* `live_bank.py:15 make`: ASRS/KTSR 기본 구성에서 cue224 bytes와 sound112 bytes를 꺼낸다. cue/sound ID는 `CRC32('SAN14_LIVE_V2_<logical>_'+SHA256(audio))`, 이름 `LIVE%03d`, codec discriminator `0x840`, sample rate48000, stream offset96 등을 쓴다. audio는 최대200MB다. SHA가 바뀌면 ID도 달라져 재생 중 이전 revision과 새 revision의 ID를 분리하는 설계다.
* `live_worker.py:59 catalog`: assets의 SHA 기반 `.ktsr/.srst`, immutable JSON snapshot을 먼저 만든다. `[Live] Version=3`, `Count=max(registered IDs, default116)+1`, `Registered=실제곡수`다. **Count는 슬롯 high-water mark이며 곡 개수가 아니다.** 이미 mapping된 파일을 덮지 않도록 기존 asset은 해시가 같으면 그대로 둔다.
* `live_worker.py:139 run`: `.catalog.ini`를 준비→`ready` 결과→최대600초 `.ack` 기다림→ack bytes가 정확히 `APPLY`면 `active.ini`를 atomic 교체한다. 이전 active와 library JSON도 보존한다. **active.ini가 단일 commit 지점**이며 legacy tracks JSON은 실패해도 committed snapshot을 읽도록 설계한다.
* native DINPUT8의 `0x37660`에는 Count·Manager Python/Script·`live/active.ini` 읽기, `0x3EE40`에는 live assets 경로/오디오 등록 코드가 있다. 이는 Python catalog와 native loader가 연결된 정적 근거다. native 오디오 객체/파일 mapping의 완전한 소유권 및 수명은 아직 미해독이다.
* `manager.py:174 apply`의 옛 offline 경로는 게임 종료, source/candidate SHA, backup/transaction journal, container→index→manifest→loader 순서를 확인한다. 현재 ZIP에 그 경로의 build/DINPUT8.dll 및 baseline stream은 들어 있지 않다. 옛 경로를 standalone 실행 가능하다고 가정하지 않는다.

## 데이터 delta와 조합

55개 GZIP을 실행 없이 압축 해제하여 **S14D1** stream을 파싱했다. 헤더는 magic5 + ASCII beforeSHA64 + afterSHA64 + little-endian Int64 출력길이 + Int32 op수다. kind0은 Int64 input offset/Int32 length의 복사, kind1은 Int32 length와 literal bytes다. CIL `ApplyDelta`는 원본 hash, 출력크기0..64MiB, op수0..1,000,000, 복사 input 범위/추가 길이, 최종 길이와 출력 SHA를 검사한다. 우리 parser는 연산형식·음수 길이·출력합·stream EOF를 검증했으며 **원본이 없는 delta의 output SHA를 재계산한 것은 아니다.**

| delta | 출력길이 | op수 | literal bytes | 대상 |
|---|---:|---:|---:|---|
| fix_regions | 85,979 | 42 | 84,156 | 0010_KO/fixdataexce.s14 |
| fix_traits | 81,994 | 102 | 78,694 | 같은 파일 |
| fix_regions_traits | 85,947 | 52 | 83,894 | 두 항목 동시 선택용 결합본 |
| tribal_events | 2,867,632 | 1,288 | 8,887 | 0004/EventPKCE001.s14 |
| tribal_messages | 321,280 | 19 | 151,930 | 0001_KO/msgdata_exce.bin |
| tribal_scenario_* 50개 | 개별 기록 참조 | 개별 기록 참조 | 개별 기록 참조 | 0010_KO/scedaexce*.s14 |

세 fix delta의 beforeSHA는 모두 `dca5784c4dd66b0ad567ee0d070c738b23785d6b26b5f43cba82fa1d1cc59b90`다. 10번/11번을 순서대로 두 개 적용하는 방식이 아니라 **처음부터 결합 delta 하나를 고른다**. 전체 대상·before/after hash·copy/literal output 구간은 [delta_inventory.json](../../research/reference_deepening/eleven_patch/delta_inventory.json)에 있다. 파일이 압축·직렬화된 형태이므로 literal 변경량을 도시/개성 record 변경량으로 환산하지 않는다. 이민족 성·지역·증축·콜라보 해금의 정확한 자료구조 및 ID별 변경 의미는 미확인이다.

## 현재 빌드와 재사용 판단

현재 디스크 `SAN14CE_KO.exe`는 30,609,888 bytes, SHA `87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a`, PE timestamp **0x6A983C25**, SizeOfImage0x29FF000이다. 설치기 whole-file hash gate와 불일치하며 Traits9/UI의 **명시적 timestamp gate도 불일치**한다. 디스크의 보호된 `.text`를 평문 native 이미지와 직접 비교한 결과는 이 노트에서 만들지 않았다. 다른 참조 노트의 plaintext capture 검증과도 출처를 구분한다. **여기에 적힌 게임 RVA·서명·ABI는 우리 현재 빌드에 그대로 쓸 수 있는 주소표가 아니다.**

현재 fixdata SHA `d3fac398d257407dcd81b0620bfd23f0956c3382c8926a5bd7ee4913da105a25`도 fix delta beforeSHA와 다르다. 현재 이벤트/메시지 파일은 해당 delta beforeSHA와 같았지만 이것만으로 다른 파일/게임 코드와의 조합을 호환이라고 판정하지 않는다. 분석에서는 어떤 game data도 갱신하지 않았다.

우리 **dinput8.dll은 입력/모듈 로더만**, **SAN14_CombatAI.dll은 전투 판단**, **SAN14_Convenience.dll은 이 노트의 인사·보고·논공·사용자 다수 이동·배속·UI·BGM·개성** 소유 후보로 유지한다. 이 참조의 통합 DINPUT8.dll을 복사하거나 새로운 version.dll 등을 추가하는 방안은 포함하지 않는다. 동일 game hook은 기존 작성자를 먼저 확인하고 한 소유자만 publish해야 한다.

재사용 우선순위는 (1) hash/변경 감지와 복원 journal, (2) immutable BGM asset+단일 commit catalog, (3) 여러 페이지를 선확보하는 patch transaction, (4) bool descriptor와 선택별 INI 생성, (5) 단계별 UI fallback이다. reference DLL에는 `SAN14CE_11Patch.log`, Traits9/UI 로그 쓰기가 실제로 들어 있다. 우리 무로그 정책에는 그 작성자를 그대로 들여오지 않는다. 라이선스/재배포 허가는 이 파일들에서 확인하지 않았으므로 구조·계약·검증 방식의 참고와 원본 코드 복제 허가를 구분한다.

## 분석 한계와 다음 조사 시작점

REA native provider는 Ghidra 환경 미설정, Hopper win32 미지원, IDA 미설정으로 pseudocode를 제공하지 못했다. [provider 오류](../../research/reference_deepening/eleven_patch/rea_native_provider_blocker.json)를 보존했다. 대신 연구 폴더에만 설치한 capstone5.0.9/pefile2024.8.26으로 PE/.pdata 범위·기계어·참조를 정적으로 해독했다. `.pdata` 1,338/6/109개 범위는 각각 DINPUT8/Traits9/UI의 unwind 범위이며 함수 개수로 단정하지 않는다. RIP 문자열/flag reference만으로 함수의 전체 의미·간접호출 ABI를 복원할 수 없다.

후속 조사자는 [DINPUT8.dll.static.json](../../research/reference_deepening/eleven_patch/DINPUT8.dll.static.json), [DINPUT8.dll.asm.txt](../../research/reference_deepening/eleven_patch/DINPUT8.dll.asm.txt), Traits9/UI의 같은 이름 자료, 원본 추출 DLL을 출발점으로 삼는다. `native_feature_refs.json`은 1~8번의 좁은 entry 후보를 제공한다. 향후 기능 차용 전에 새 게임 빌드의 평문 코드·caller·행동 조건·객체 수명·저장 호환성을 별도로 검증해야 한다.

완료한 자체 검증은 원본 SHA, payload59개의 정확한 원본 byte range/해시, GZIP55개 재현, BGM ZIP22개 CRC, Python5개 AST parse, delta55개 구조/연산합/EOF다. 전부 context-mode의 정적 parser로 수행했으며 결과는 `verification_static.json`에 보존했다. 제공 회귀 실행이나 실제 게임 테스트를 실행한 결과는 없다.
