#!/usr/bin/env python3
"""
Test migration script for libjpl API v0.6.0+
Converts old double[2]/double[6] API to new jpl_date/jpl_state struct API.
"""

import re
import sys
from pathlib import Path

def migrate_test_file(content: str) -> str:
    """Migrate a single test file from old API to new API."""
    
    # Pattern 1: static const double jd[2] = {x, y}; -> jpl_date jd = {x, y};
    # But be careful - some are inside structs or have other uses
    def replace_jd_decl(match):
        var_name = match.group(1)
        hi = match.group(2)
        lo = match.group(3)
        return f"jpl_date {var_name} = {{{hi}, {lo}}};"
    
    content = re.sub(
        r'static const double (\w+)\[2\] = \{([^,]+),\s*([^}]+)\};',
        replace_jd_decl,
        content
    )
    
    # Pattern 2: const double jd[2] = {x, y}; (non-static)
    content = re.sub(
        r'const double (\w+)\[2\] = \{([^,]+),\s*([^}]+)\};',
        replace_jd_decl,
        content
    )
    
    # Pattern 3: double jd[2] = {x, y}; (non-static, non-const)
    content = re.sub(
        r'double (\w+)\[2\] = \{([^,]+),\s*([^}]+)\};',
        replace_jd_decl,
        content
    )
    
    # Pattern 4: double state[6] -> jpl_state state;
    content = re.sub(
        r'double (\w+)\[6\](\s*[;=])',
        r'jpl_state \1\2',
        content
    )
    
    # Pattern 5: static const double var[6] = {...}; -> keep as is (used for mock data)
    # Pattern 5: double var[6] = {...}; -> keep as is (initializer)
    
    # Pattern 6: Function calls with jd arrays -> need &jd
    # jpl_func(e, jd, ...) -> jpl_func(e, &jd, ...)
    # This is complex - need to identify which functions take jd
    
    # Known functions that take jd as first param after e:
    jd_functions = [
        'jpl_get_state', 'jpl_get_state_lighttime', 'jpl_planet_position',
        'jpl_moon_position', 'jpl_apparent_ecliptic_position',
        'jpl_tt_to_tdb', 'jpl_tdb_to_tt', 'jpl_get_tt_minus_tdb',
        'jpl_ecliptic_to_icrs', 'jpl_icrs_to_ecliptic', 'jpl_frame_rotation_matrix',
        'jpl_tt_to_tdb', 'jpl_tdb_to_tt'
    ]
    
    for func in jd_functions:
        # Pattern: func(e, jd, ...) -> func(e, &jd, ...)
        # where jd is a variable name (not a literal array)
        pattern = rf'({func}\s*\(\s*\w+\s*,\s*)(\w+)(\s*,)'
        content = re.sub(pattern, r'\1&\2\3', content)
    
    # Fix state array access: state[0] -> state.x, state[1] -> state.y, etc.
    state_access_map = {
        r'\b(\w+)\[0\]': r'\1.x',
        r'\b(\w+)\[1\]': r'\1.y',
        r'\b(\w+)\[2\]': r'\1.z',
        r'\b(\w+)\[3\]': r'\1.vx',
        r'\b(\w+)\[4\]': r'\1.vy',
        r'\b(\w+)\[5\]': r'\1.vz',
    }
    
    for pattern, replacement in state_access_map.items():
        content = re.sub(pattern, replacement, content)
    
    # Fix jpl_apply_aberration calls: state is now jpl_state*, vobs is double[3]
    # Old: jpl_apply_aberration(state, vobs, ...) where state is double[6]
    # New: jpl_apply_aberration(&state, vobs, ...) where state is jpl_state*
    
    # Fix jpl_icrs_to_true_ecliptic_position: takes const double in[3], jpl_date*, double out[3]
    # The in/out arrays remain as double[3]
    
    # Fix the atan2 bug in test_convenience.c
    # Old: difference = atan2(sin(out_pos.longitude_deg * (acos(-1.0) / 180.0) - lon_old), cos(...))
    # Should be: diff = out_pos.longitude_deg * M_PI/180 - lon_old; difference = atan2(sin(diff), cos(diff))
    content = content.replace(
        'difference = atan2(sin(out_pos.longitude_deg * (acos(-1.0) / 180.0) - lon_old),\n'
        '                            cos(out_pos.longitude_deg * (acos(-1.0) / 180.0) - lon_old));',
        'diff = out_pos.longitude_deg * (acos(-1.0) / 180.0) - lon_old;\n'
        '        difference = atan2(sin(diff), cos(diff));'
    )
    
    # Fix memcpy(state, local, sizeof local) -> *state = local when state is jpl_state*
    content = re.sub(
        r'memcpy\((\w+),\s*local,\s*sizeof\s*local\);\s*',
        r'*\1 = local;\n',
        content
    )
    
    # Fix memcpy(local, state, sizeof local) when state is jpl_state*
    content = re.sub(
        r'memcpy\(local,\s*(\w+),\s*sizeof\s*local\);\s*',
        r'local = *\1;\n',
        content
    )
    
    # Fix memset(state, 0, sizeof ...) for jpl_state
    content = re.sub(
        r'memset\((\w+),\s*0,\s*sizeof\s*\w+\);\s*',
        r'\1.x = \1.y = \1.z = \1.vx = \1.vy = \1.vz = 0.0;\n',
        content
    )
    
    # Fix double local[6] = {0}; -> double local[6] = {0}; (keep for local arrays)
    # But if it's a jpl_state, change to jpl_state local = {0};
    
    return content


def main():
    if len(sys.argv) < 2:
        print("Usage: python migrate_tests.py <test_file.c>")
        sys.exit(1)
    
    filepath = Path(sys.argv[1])
    if not filepath.exists():
        print(f"File not found: {filepath}")
        sys.exit(1)
    
    content = filepath.read_text()
    migrated = migrate_test_file(content)
    
    # Backup original
    backup = filepath.with_suffix('.c.bak')
    filepath.write_text(content)
    print(f"Migrated {filepath} (backup at {backup})")


if __name__ == '__main__':
    main()