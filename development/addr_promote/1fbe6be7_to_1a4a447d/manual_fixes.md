# Fixes that the codemod cannot derive (W1-E, found by running the fixtures)
- Fixture image sizes 0x29FF000 -> 0x2A0B000 (advisor briefing, diplomacy, split module, damage preview, crash diagnostic); src/native_advisor_briefing.inl plausibility bound the same.
- Page bases and page-aligned VirtualProtect literals that follow a moved site: native_fixture (4 pages), injury_fixture (0x2EC00 -> 0x2F760), personnel_native_fixture (0x35CA00 -> 0x35EEF0, 0x34A890 -> 0x34CB50), friendship (id + offset repeated on one line).
- Byte strings: split_module_fixture 8E1D90 call, crash_diagnostic_fixture game_rva string.
- tests/merit_combat_capture.h: captured native functions re-captured from the 1a4a447d image (each located by its masked bytes: 2A6AA0->2A8C90, 2A6B60->2A8D50, 2AF7E0->2B1B10, 2C8180->2CA530, 21B2E0->21CCC0); call and data targets mapped by instruction alignment (merit_combat_fixture.inl test_captured_combat).
- native_adapter.cpp integrated install(): four slot literals with u suffix -> GA(GA_x) (check_addresses now sees u-suffixed literals).
- sortie_entries[GA_2B2550].size 5 -> 6 (the 1a4a447d prologue is push rdi/r13/r14; size 5 cut an instruction), speed_tail pops adapted.
- verify_modpack_static / verify_callsite_compat: blob RVAs mapped through idmap.tsv, old-only checks removed.
