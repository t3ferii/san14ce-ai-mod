# 공개 소스 스냅샷

기준: v15(`SAN14_AI_v15`, 2026-11-01). 공개 v15 DLL·설치기를 빌드한 작업 폴더의 소스다. 이 파일은 개인 설치 상태를 나타내지 않는다.

DLL은 네 개다: `dinput8.dll`(로더), `SAN14_CombatAI.dll`(전투), `SAN14_Convenience.dll`(나머지·게시 조정), `SAN14_CCZ.dll`(조조전 탭, 편의 DLL이 필요할 때 불러옴). 개발 후보·미연결/이전 설계도 존재한다. 파일의 존재만으로 기능 활성화·설치·실게임 검증을 추정하지 않는다. 현재 실행 경로는 build defines, MODULES, native_split_install.inl을 함께 확인한다.

[빌드 안내](../docs/SOURCE_BUILD.md) · [누락 외부 의존성](../docs/SOURCE_DEPENDENCIES.md) · [이번 공개 검증](../docs/SOURCE_VALIDATION.md)
