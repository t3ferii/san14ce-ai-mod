# ga_equiv baseline (W1-C, docs/MODULARIZATION_PLAN.md)

`baseline_b1/` = the dump taken on main (a286539) BEFORE the address table was re-based: table ids named by old-build RVAs, build index 1 = the current Steam build 1a4a447d, exact-byte guards (ga_expb) and hash pairs in effect.
The dump holds no ids, only values the code reads (V), effective guard expectations (M/A/H/G), denied source files (D) and table values (T). `nonaux.txt` = table values of that build that are not auxiliary containers.

`baseline_b1_data/` = the same plus the two data-range guards (A 13D70E8, A 142FC70) that the pre re-base code evaluated against the source literal without dumping them (see its README.txt). It is the folder to compare against.

Result of the W1-E re-base (tools/addr_promote.py, 2026-11-07): after the re-base and the 1-F deletions

    powershell -File tools/build_ga_equiv_dump.ps1 -Out <dir>
    python tools/ga_equiv_compare.py development/ga_equiv/baseline_b1_data <dir> --nonaux development/ga_equiv/baseline_b1/nonaux.txt \
        --ignore-single modpack_clamp_restore_safe --ignore-line "V 231AFB"

prints EQUIVALENT. Both ignores name code deleted by 1-F (the previous-build clamp-form guard and the read of its long-form byte row). Without the 1-F deletions (commit "W1-E: addr_promote codemod") no ignore is needed.
At the next patch the dump taken before the promotion is the new baseline.
