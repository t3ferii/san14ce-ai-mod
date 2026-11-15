# 조조전 부대 형상 자산

- `ccz_units.bin` — CCZU v2(0-연속 RLE, DLL이 직접 푼다). **신조조전** 병종 형상 570칸(병종 190 × 진영 색 3칸, 칸 = 병종×3; 이름표 research/cao_cao_den/tables/classes_sin.csv), 칸마다 이동 11·공격 12·특수 5프레임(방어·피격·승리). 8비트 색 번호 + 팔레트 + 팀색 표시. 읽는 곳: `src/native_ccz_sprites.inl`. 형식은 [tools/ccz_extract_units.py](../../tools/ccz_extract_units.py) 머리말.
- `ccz_units.json` — 원본 파일 SHA-256, 결과 해시, 팀색 자리 목록.
- `preview_classes.png` — 각 칸 이동 0번(앞 걸음) 프레임. 색은 미리보기용 색상 회전이다.
- 다시 만들기: `python -I -X utf8 tools/ccz_extract_units.py 0 570 --src=신조조전 --generals` (장수 전용 병종 표 `data/ccz_generals.tsv`, 칸은 0~569 안에 있음) (원본과 바이트 단위 되읽기 검사 포함). `preview_classes.png`는 이전 조조전 240칸 미리보기라 현재 자산과 다르다.

## 출처·크레딧

조조전(KOEI 원작) 기반 **신조조전 7.0 (Ekd5 엔진, 게임 폴더 `신조조전`, 2015 빌드)** 의 `Unit_mov.E5`, `unit_atk.e5`, `Unit_spc.E5`, `Spalet.e5`. 2026-10-31 사용자 지정으로 이전 `조조전`(6.6, 병종 80)에서 바꿨다.
사용자 판단(2026-10-31): 조조전 자산은 저작권 제약이 없는 상태이므로 모드에 포함한다. 배포할 때는 크레딧 규칙에 따라 README·읽어주세요·설치기 설명에 출처를 적는다.
