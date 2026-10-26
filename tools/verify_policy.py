"""Run the newly authored policy DLL against user-behavior scenarios.
Does not open the game process, call game code, or modify a game file.
"""
from pathlib import Path
import ctypes as C
import itertools
import json
import hashlib

ROOT = Path(__file__).resolve().parents[1]


class Situation(C.Structure):
    _fields_ = [(name, C.c_int if name != 'legal_facilities' else C.c_uint)
                for name in ['computer_controlled', 'valid_hostile_player_cut',
                             'captured_region_in_campaign', 'active_legal_construction',
                             'legal_facilities', 'feasible_other_task', 'feasible_city_attack']]


class Decision(C.Structure):
    _fields_ = [('action', C.c_int), ('facility_id', C.c_int)]


def main():
    dll_path = ROOT / 'build/SAN14_TacticsPolicy.dll'
    lib = C.CDLL(str(dll_path))
    lib.tactics_area_coefficient.argtypes = [C.c_int] * 4
    lib.tactics_area_coefficient.restype = C.c_int
    lib.tactics_choose_facility.argtypes = [C.c_uint]
    lib.tactics_choose_facility.restype = C.c_int
    lib.tactics_decide.argtypes = [C.POINTER(Situation), C.POINTER(Decision)]
    lib.tactics_decide.restype = C.c_int
    tests = []

    for native in [4, 16, 24, 32]:
        assert lib.tactics_area_coefficient(6, 2, 7, native) == 4
        assert lib.tactics_area_coefficient(6, 2, 2, native) == native
        assert lib.tactics_area_coefficient(2, 2, 6, native) == native
        assert lib.tactics_area_coefficient(6, 2, 0, native) == 4
    assert lib.tactics_area_coefficient(6, 2, -1, 24) == -1
    tests.append('AI-vs-AI fixed 4; actual player opponent retains native coefficient; unknown context rejected')

    # An illegal expensive facility cannot prevent falling back to a lower one.
    priorities = [6, 4, 7, 3, 11, 12, 14, 13]
    for availability in itertools.product([0, 1], repeat=len(priorities)):
        enabled = [f for f, yes in zip(priorities, availability) if yes]
        mask = sum(1 << f for f in enabled)
        assert lib.tactics_choose_facility(mask) == (enabled[0] if enabled else 0)
    assert lib.tactics_choose_facility((1 << 1) | (1 << 2) | (1 << 9)) == 0
    tests.append('256 availability combinations; facility fallback and unsupported types')

    def choose(values):
        result = Decision()
        assert lib.tactics_decide(C.byref(Situation(*values)), C.byref(result)) == 1
        return result.action, result.facility_id

    assert choose([0, 1, 1, 1, 1 << 6, 1, 1]) == (0, 0)
    assert choose([1, 1, 1, 1, 1 << 6, 1, 1]) == (1, 0)
    assert choose([1, 0, 1, 1, 1 << 6, 1, 1]) == (2, 0)
    assert choose([1, 0, 1, 0, (1 << 6) | (1 << 4), 1, 1]) == (3, 6)
    assert choose([1, 0, 0, 0, 1 << 6, 1, 1]) == (4, 0)
    assert choose([1, 0, 1, 0, 0, 1, 1]) == (4, 0)
    assert choose([1, 0, 1, 0, 0, 0, 1]) == (5, 0)
    assert choose([1, 0, 1, 0, 0, 0, 0]) == (6, 0)
    assert lib.tactics_decide(None, None) == 0
    tests.append('Cut precedes construction; construction precedes other tasks; city attack is last fallback; player orders preserved')
    report = {'status': 'policy_component_passed', 'game_integration': 'separate_native_adapter_requires_gameplay_validation',
              'gameplay_verified': False, 'dll_sha256': hashlib.sha256(dll_path.read_bytes()).hexdigest(),
              'tests': tests}
    (ROOT / 'build/policy_verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
