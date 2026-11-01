"""Prepare an isolated, text-only public source snapshot; never install or upload.

Run from any directory: python -B -X utf8 tools/export_source_snapshot.py
The allowlist deliberately excludes game assets, binary payloads and user data.
AI comments describe existing contracts; executable C++ tokens are checked equal.
"""
from pathlib import Path
import ast
import hashlib
import json
import re
import sys

PROJECT = Path(__file__).resolve().parents[1]
# Usage: export_source_snapshot.py [destination] [snapshot label]
DEST = Path(sys.argv[1]).resolve() / 'AI_TacticsMod' if len(sys.argv) > 1 else PROJECT / 'build/development/source_public_20261026_r3/AI_TacticsMod'
LABEL = sys.argv[2] if len(sys.argv) > 2 else '2026-10-26 development source; not an exact v13.3 release reproduction'
TEXT = {'.cpp', '.c', '.h', '.inl', '.asm', '.hlsl', '.py', '.ps1', '.psm1', '.manifest', '.ini', '.txt', '.wprp', '.cmd', '.cs'}
EXTRA_TOOLS = '''check_feature.ps1 import_modpack_objects.py prepare_traits9.py traits9_layout.py
install_local.py install_damage_preview.py package_mod.py package_input.py install_split.py
generate_handoff.py audit_current_source.py generate_unified_labels.py prepare_modpack_tables.py
prepare_public_release.py export_source_snapshot.py release_features.py'''.split()
DOCS = '''MODULES.md NATIVE_FUNCTIONS.md FEATURE_SWITCHES.md WORKFLOW.md SESSION_RULES.md
DAMAGE_PREVIEW.md BUILDING_DESTRUCTION.md COMBAT_AI.md DOMESTIC.md EXPERT_SETTINGS.md
UNIFIED_PERSONNEL.md ADVISOR_BRIEFING.md DIPLOMACY_INTEL.md CREDITS.md CCZ_SPRITES.md PUBLISH.md'''.split()

# Contract annotations belong next to the implementation, rather than in a
# duplicated implementation summary. Anchors must match exactly once.
COMMENTS = {
 'src/native_adapter.cpp': [
  ('using Obj = unsigned char *;', 'AI-CONTRACT [ABI]: Obj is a borrowed native object, never an owning buffer. Every offset/RVA is build-specific. Preserve __fastcall signatures and validate caller + original bytes before changing bindings.'),
  ('static int mod_setting(', 'AI-CONTRACT [settings]: Maintain the unified INI lookup and legacy fallback. A new exposed key must be added to InstallerGui.cs and distribution/FeatureSelect.ps1 in the same patch; keep parent && child gating.'),
  ('struct ModFeatures {', 'AI-CONTRACT [variants]: This translation unit builds twice with SPLIT_BASE/SPLIT_COMBAT. Follow native_split_install.inl to prove reachability and owner; a declaration or fixture alone does not establish runtime activation.')],
 'src/module_loader.cpp': [
  ('static DWORD WINAPI initialize_modules(', 'AI-CONTRACT [lifecycle]: This worker negotiates the split ABI and publishes validated hooks only. Do not add gameplay getters/orders here. Module handles stay resident because callbacks can remain in flight.')],
 'src/split_module_api.h': [
  ('struct SplitCombatApi {', 'AI-CONTRACT [ABI]: Field order, calling convention, sizeof checks and split_module_abi are a cross-DLL contract. Change both producer and consumer together; test bad-ABI and partial-publication rollback cases.')],
 'src/native_split_install.inl': [
  ('static const SplitPointer split_pointers[]={', 'AI-CONTRACT [ownership]: Each original slot/site has one DLL owner. Check this table and conditional CodeLink paths together before adding a hook; shared helper code does not authorize a second publisher.'),
  ('static void split_feature_guards() {', 'AI-CONTRACT [compatibility]: New hook sites need feature-specific original-byte guards and a unique split_disable diagnostic ID. A mismatch disables that feature; do not replace these checks with a whole-image CRC requirement.')],
 'src/native_path_fast.inl': [
  ('static path_fast_core::Storage *volatile slot_storage[3];', 'AI-CONTRACT [lifetime]: Storage ownership is per native buffer slot, not per thread. Native force-AI threads are short-lived; thread_local heap ownership caused the v13.1/v13.2 leak. Preserve slot critical-section evidence before changing reuse.'),
  ('static constexpr unsigned clear_site=0x184485;', 'AI-CONTRACT [hook boundary]: 183A30/184485 belong to convenience; 1843E0 entry belongs to combat. Preserve mutual exclusion with path_count and native-search reentrancy guards; delegation must restore native node state.')],
 'src/path_fast.h': [
  ('struct Node {', 'AI-CONTRACT [layout]: This is the x64 native node ABI, not a freely replaceable container. Keep offset/size assertions and signed-wrap semantics; validate clean waypoint segments, reopen behavior and bounded result assembly in path_fast_fixture.cpp.')],
 'src/native_campaign.inl': [
  ('static constexpr int campaign_stationary_days=40;', 'AI-CONTRACT [policy]: Game days use the native 30-day calendar. Preserve actual hostile castle/gate damage and cohort-only 30-day forced-march exceptions before ordinary 40-day stationary retreat. Do not infer a siege lock from an attack order alone.'),
  ('static bool campaign_individual_priority(Obj army,Progress &p) {', 'AI-CONTRACT [writer order]: Ordering is behavior. Audit competing command writers, same-day callbacks, goal capture, identity reuse and direct-player ownership before moving a branch; compare campaign priority/operation fixtures.')],
 'src/native_campaign_routes.inl': [
  ('static bool campaign_edge(unsigned from,unsigned to) {', 'AI-CONTRACT [graph]: Adjacency is enabled Area edges, not geometric tile proximity. Preserve force hostility and valid-area filtering; an apparent route is not permission to issue a movement command.')],
 'src/native_sortie.inl': [
  ('using SortieMovement=float(__fastcall *)(Obj,Obj,int,int,Obj);', 'AI-CONTRACT [caller ABI]: The fifth argument is a tile pointer captured from the native stack contract. Preserve pointer width and nonvolatile state; a fixture matching a guessed signature is not independent native evidence.')],
 'src/native_merit_save.inl': [
  ('static merit::SaveCoordinator merit_jobs;', 'AI-CONTRACT [save epochs]: Native save/load worker callbacks may hash archive bytes only. Relation changes and native person access stay on host callbacks; retain epoch identity, binding and stale-job rejection when editing sidecar flow.')],
 'src/native_save_codec.inl': [
  ('static unsigned long long __fastcall encode(', 'AI-CONTRACT [save compatibility]: Keep decode-and-compare before committing replacement output and retain original-encoder fallback. Match the native allocator and ownership for returned buffers; a faster stream must remain readable by the unmodified decoder.')],
 'src/native_damage_preview.inl': [
  ('namespace damage_forecast {', 'AI-CONTRACT [read-only UI]: Forecast hooks may compute/display damage but must never issue commands or apply casualties/durability. Preserve target-kind discrimination; verify the UI caller ABI independently of pure arithmetic fixtures.')],
 'src/native_building_destruction.inl': [
  ('static thread_local Context *current;', 'AI-CONTRACT [nested calls]: This pointer is a borrowed per-call context, not persistent target ownership. Preserve restoration on nested/failed calls, native failure semantics and expansion only after successful primary completion; targets exclude cities/gates.')],
 'src/native_expert_settings.inl': [
  ('static std::map<Obj,std::array<ExpertRow,2>> expert_rows;', 'AI-CONTRACT [UI lifetime]: Map keys are native layout identities. Preserve lifecycle cleanup and commit/cancel/save semantics; row bytes and screen layout records must agree. Successful static checks do not prove rendering/click behavior.')],
 'src/native_modpack.inl': [
  ('static bool prepare_imported(const ImportedCode &code) {', 'AI-CONTRACT [external provenance]: Payload code is imported from fingerprinted third-party DLLs, not reconstructed C++. Keep exception-table registration, section protection and expected hashes; project permission does not relicense the external payload.')],
}


def cpp_tokens(text):
    # Match string/char/raw literals before comments so URLs and escaped quotes
    # cannot be accidentally treated as comments by the publication check.
    pattern = r'R"(?P<raw>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=raw)"|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u8|u|U|L)?\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|\s+|.'
    return [m[0] for m in re.finditer(pattern, text) if not m[0].isspace() and not m[0].startswith(('//', '/*'))]


def main():
    if DEST.exists():
        raise SystemExit('Snapshot already exists; preserve it and choose a new export path.')
    selected = set()
    for folder in ('src', 'tests', 'installer', 'distribution'):
        selected.update(p for p in (PROJECT / folder).rglob('*') if p.is_file() and p.suffix in TEXT)
    selected.update(p for p in (PROJECT / 'tools').glob('*') if p.is_file() and (p.name.startswith(('build_', 'verify_'))) and p.suffix in TEXT)
    selected.update(PROJECT / 'tools' / name for name in EXTRA_TOOLS)
    selected.update(PROJECT / 'docs' / name for name in DOCS)
    selected.update([PROJECT / 'development/features.json', PROJECT / 'README.md', PROJECT / 'legacy/FogMod_Source/tools/audit_native_game.py'])
    # Resolve local Python helper imports without executing any module.
    while True:
        more = set()
        for path in selected:
            if path.suffix != '.py':
                continue
            for node in ast.walk(ast.parse(path.read_text(encoding='utf-8-sig'))):
                names = [node.module] if isinstance(node, ast.ImportFrom) and node.module else [a.name for a in node.names] if isinstance(node, ast.Import) else []
                for name in names:
                    for parent in (PROJECT / 'tools', PROJECT / 'legacy/FogMod_Source/tools'):
                        candidate = parent / (name.split('.')[0] + '.py')
                        if candidate.is_file():
                            more.add(candidate)
        if more <= selected:
            break
        selected |= more
    records = []
    for source in sorted(selected):
        rel = source.relative_to(PROJECT).as_posix()
        raw = source.read_bytes()
        text = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
        # Public templates come from distribution, never the user's live INI.
        if source.suffix == '.py':
            text = re.sub(r'^([ \t]*)sys\.path\.insert\(0,\s*r?[\'\"]C:\\Users\\[^\n]+\n', r'\1pass  # Public snapshot: install capstone in the active Python environment.\n', text, flags=re.M)
        # Remaining author-machine analysis paths keep their shape, not the account name.
        text = re.sub(r'C:([\\/])Users\1[^\\/\'"\s]+', r'C:\1Users\1<user>', text)
        for anchor, contract in COMMENTS.get(rel, []):
            if text.count(anchor) != 1:
                raise ValueError('Ambiguous annotation anchor: ' + rel + ' / ' + anchor)
            text = text.replace(anchor, '// ' + contract + '\n' + anchor, 1)
        if source.suffix in {'.cpp', '.c', '.h', '.inl'}:
            if cpp_tokens(raw.decode('utf-8-sig')) != cpp_tokens(text):
                raise ValueError('Executable tokens changed: ' + rel)
        if rel != 'tools/export_source_snapshot.py' and re.search(r'C:[\\/]Users[\\/](?!<user>)', text):
            raise ValueError('Private machine path remains: ' + rel)
        target = DEST / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')
        records.append(dict(path=rel, original_sha256=hashlib.sha256(raw).hexdigest(), published_sha256=hashlib.sha256(target.read_bytes()).hexdigest(), ai_contracts=len(COMMENTS.get(rel, []))))
    template = DEST / 'distribution/기능설정.ini'
    (DEST / '기능설정.ini').write_bytes(template.read_bytes())
    report = dict(snapshot=LABEL, files=len(records), ai_contracts=sum(r['ai_contracts'] for r in records), cpp_executable_tokens_unchanged=True, excluded=['game binaries/assets', 'third-party payloads', 'save data', 'logs', 'backups', 'runtime settings', 'research dumps'], files_detail=records)
    (DEST.parent / 'export_audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files_detail'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
