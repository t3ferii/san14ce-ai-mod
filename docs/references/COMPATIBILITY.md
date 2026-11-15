# 빌드·로더·안개 참고

현재 게임 `SAN14CE_KO.exe` SHA256:
`87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a`.

| 자료 | 지원 근거 / 결론 |
|---|---|
| [gamebuild.py](../../../reference_mod/Editor_Source/san14ce/gamebuild.py):25–34 | 2026-09-28, timestamp `0x6A983C25`, ImageSize `0x029FF000`, Steam SHA가 **현재 EXE와 일치**. 평문 EXE SHA는 별도 |
| [AI 조사 빌드 표](../../../reference_mod/Modding_DOCS/memory/AI_AND_INTERNAL_AFFAIRS.md):9–28 | 09-10→09-28 코드 주소가 이동; vtable도 변함. 새 주소 없는 행은 자동 이식하지 않음 |
| CaptiveDialog/TraitHover/Stats200 C | 검사 코드가 옛 timestamp `0x6A6D9F72`를 요구. 설계만 참고하고 RVA/가드 재검증 필요 |
| R15/R17, RC25, RelationTab, 11Patch | manifest/설명에 지원 EXE SHA `07a047…3aa40`를 지정. 현재 SHA와 **다름**; 자동 호환으로 판단하지 않음 |
| [안개1.6 package](<../../../reference_mod/SAN14CE 전장의 안개 1.6/_files/package.json>) | timestamp `0x6A983C25`, ImageSize `0x029FF000`; `live_game_verified=false`와 미확인 항목을 명시. 전체 EXE 해시/실제 훅 호환의 증명은 아님 |

재사용할 구조:

- [locator.py](../../../reference_mod/Editor_Source/san14ce/locator.py)의 루트/vtable/표 가드: 필드 번호와 객체 타입을 함께 확인.
- [codepatch.py](../../../reference_mod/Editor_Source/san14ce/codepatch.py), [codecave.py](../../../reference_mod/Editor_Source/san14ce/codecave.py), [RUNTIME_PATCH 문서](../../../reference_mod/Modding_DOCS/memory/RUNTIME_PATCH_AND_CODECAVE.md): 예상 원본 바이트·중복 적용·충돌 검출·원복 기록. 외부 쓰기/스레드 정지 실행 코드는 우리 작업 정책으로 가져오지 않음.
- [hover_apply.h](../../../reference_mod/SAN14CE_TraitHover_Standalone_v1_0/SAN14CE_TraitHover_Standalone_v1_0/source/hover_apply.h): 전체 signature/원본 함수포인터 확인, 보호 변경 후 재검사, 완성된 relay를 먼저 게시, 캐시 flush/보호 복원.
- [4Mods 소스 설명](<../../../reference_mod/SAN14CE_4Mods_v1_3/01_포획대사/source/README_SOURCE_KO.txt>): `reference/`는 보존 소스이며 현재 배포 DLL과 같다고 보장하지 않음. 옛 시험은 새 배포판 검증으로 재사용하지 않음.

안개 알고리즘은 [FOG_PATCH_245_REVIEW](../../../reference_mod/Modding_DOCS/memory/FOG_PATCH_245_REVIEW.md)의 **2.4.5**와 기존 우리 `FogMod_Source`를 출발점으로 쓴다. 참고 폴더 **1.6** DLL의 알고리즘을 같은 것으로 확정한 것은 아니다. 문서에 나온 `root+0xE270` 지도 소유 정보·시야/UI 훅을 기능별로 좁혀 읽는다.

DLL만 있는 모드는 이번 조사에서 전체 역어셈블하지 않았다. 요청 기능이 겹칠 때 해당 DLL 해시·export/문자열·관련 호출만 추가 분석하고 근거를 남긴다. 원본 모드의 EXE/DLL/설치기·테스트는 실행하지 않았다. 설치/공유 파일을 덮어쓴 결과물도 없다.
