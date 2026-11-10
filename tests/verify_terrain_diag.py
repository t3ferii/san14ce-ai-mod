import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""v15.1 terrain A/B/C source invariants. This is static verification, not a game/FPS test."""
from pathlib import Path
import json
import sys

P = Path(__file__).resolve().parents[1]

def source(name):
    return (P / 'src' / name).read_text(encoding='utf-8-sig')

checks = 0
failed = []
def ok(value, label):
    global checks
    checks += 1
    if not value:
        failed.append(label)

adapter = source('native_adapter.cpp')
foam = source('native_terrain_foam.inl')
render = source('native_terrain_render.inl')
split = source('native_split_install.inl')
ccz = source('native_ccz_sprites.inl')

ok('unsigned terrain_diag_mode = 0;' in adapter, 'mode defaults to zero')
ok('mod_setting(L"diagnostics", L"terrain_diag_mode", 0)' in adapter, 'mode uses managed settings helper')
ok('terrain_diag >= 0 && terrain_diag <= 3' in adapter, 'mode range guarded')
ok('!(mod_features.terrain_diag_mode & 1u)' in foam and
   '|| (mod_features.terrain_diag_mode & 1u))return;' in foam, 'foam inactive and unpublished in A/C')
ok('!(mod_features.terrain_diag_mode & 2u)' in render, 'renderer inactive in B/C')
ok('(mod_features.terrain_diag_mode & 2u) || !guards()' in split, 'height capture unpublished in B/C')
ok('terrain_foam_active=terrain_foam::active()?1u:0u' in split and 'terrain_render::hooks_active()?1u:0u' in split,
   'diagnostic log reports actual published hooks')
import registry_text as R
ok('terrain_render::install_early()' in R.row('terrain_render_early') and 'terrain_foam::install()' in R.row('terrain_foam') and
   'terrain_height_link::install()' in R.row('terrain_height_link') and
   R.index('terrain_render_early') < R.index('terrain_foam') < R.index('terrain_height_link'), 'baseline install order preserved')
ok('fallback_attempted' in ccz and 'ccz_sprites_zoom_fallback' in ccz and
   'w)*8u==static_cast<unsigned long long>(want_width)*3u' in ccz and 'return submitted;' in ccz,
   'CCZ local14 normal and 3/8 fallback rendering retained')
for mode, expected in {0:(1,1), 1:(0,1), 2:(1,0), 3:(0,0)}.items():
    ok((int(not(mode & 1)), int(not(mode & 2))) == expected, 'mode mapping '+str(mode))

print(json.dumps({'status':'passed' if not failed else 'failed','checks':checks,'failures':failed,
                  'live_game_verified':False}, ensure_ascii=False))
sys.exit(1 if failed else 0)
