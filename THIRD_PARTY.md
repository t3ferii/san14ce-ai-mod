# 출처와 라이선스 범위

프로젝트 독자 소스의 수정·재배포는 LICENSE의 MIT 허용을 따른다. LICENSE는 타인의 저작물이나 원작 게임 자료에 대한 권리를 부여하지 않는다. 기존 원작·참고 자료의 권리 표시를 지우지 않는다.

- 삼국지14/14PK의 게임 코드·자료·명칭·화면 및 기존 스크린샷의 권리는 각 권리자에게 있다. 게임 EXE/RDB/bank·메모리 dump·세이브는 이번 소스 공개에서 제외했다. 바이트 가드·RVA·구조체 설명은 해당 지원판과의 호환 근거이며 원작 게임 코드의 재라이선스를 뜻하지 않는다.
- `src/native_modpack.inl`, `modpack_tables.h`와 관련 도구는 기존 ModPack v1.6의 tooltip/전사 구현 및 패치 근거와 연결된다. 두 외부 DLL은 고정 SHA-256을 검사해 COFF로 변환·정적 링크한다. 그 내부 구현은 독자 C++ 소스로 공개된 것이 아니며 외부 DLL·추출 OBJ·생성 payload header는 저장소에 포함하지 않는다. 원래 작성자 표시·이용 조건은 외부 원본을 따른다. 저장소는 확인되지 않은 재배포 허가를 주장하지 않는다.
- `src/native_traits9.inl`과 `tools/prepare_traits9.py`는 외부 `Editor_Source/san14ce`의 선택 정의를 이용하여 manifest와 9개성 화면 자료를 생성한다. 원본 에디터 소스와 생성된 원작 자료/payload는 미포함이다. 독자 wrapper 공개가 외부 에디터의 권리를 변경하지 않는다.
- Microsoft SDK·C++/.NET 도구, capstone 등의 도구/라이브러리는 각 배포자의 조건을 따른다. 개발 PC의 도구 바이너리나 설치 파일은 포함하지 않는다.

외부 자료가 필요한 위치와 지문은 [SOURCE_DEPENDENCIES](docs/SOURCE_DEPENDENCIES.md)를 따른다. 독자 변경본을 공유할 때도 본 라이선스와 외부 출처를 유지하고, 원본 배포와 구별되는 버전/변경 내용을 표시한다.
