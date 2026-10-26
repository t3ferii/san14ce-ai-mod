# Public mod source: AI editing contract

Read `docs/SOURCE_BUILD.md`, `docs/AI_EDITING.md`, the relevant `docs/MODULES.md` row and `development/features.json` before editing. This is a development snapshot, not a verified reproduction of the v13.3 release. Experimental code can be present without being enabled.

- `dinput8.dll`: input forwarding/module loader only. `SAN14_CombatAI.dll`: combat decisions. `SAN14_Convenience.dll`: other features and publication coordinator. One hook/site/vtable slot has exactly one owner; inspect `src/native_split_install.inl` and build defines together.
- Run native game functions only from their original UI/AI callback threads. Loader workers may validate/publish hooks, never enumerate game objects or issue commands. No external live-memory writes or remote calls.
- Preserve original game EXE, saves, references and existing backups. Do not run historical installers or reference binaries. Use `tools/install_local.py` only after its documented installed-package prerequisites are met; a clone alone is not an installable package.
- Follow `AI-CONTRACT` comments at the relevant implementation. Raw pointers/RVAs are build-specific. New hooks need byte/ABI/caller evidence, per-feature guards, one owner, rollback and mismatch fixtures. Do not suppress guards to make a different patch load.
- Settings belong in `기능설정.ini`. Add exposed keys to `installer/InstallerGui.cs` and `distribution/FeatureSelect.ps1` in the same change. The game-folder FeatureSelect copy is produced by installation. Preserve parent/child AND gating, save data and unknown settings.
- Use existing `log_event`; release code excludes diagnostics with `NO_RUNTIME_LOG`. Do not add runtime DLLs, background tools or parallel log formats.
- `tools/build_public_smoke.ps1` needs no game assets. Full checks use `tools/check_feature.ps1 -Feature <name> -List`, the corresponding route and `-Feature dll-split -Build` after providing private dependencies. Test/build output is not live-game verification.
- Keep changes local to the requested feature. With concurrent sessions, agree ordering before editing common source/hooks/install files; never revert another session's changes. Do not spawn subagents unless explicitly requested.
- Respect `LICENSE` and `THIRD_PARTY.md`: third-party imported code, game data and binary-derived content are not relicensed by this repository's MIT permission.
