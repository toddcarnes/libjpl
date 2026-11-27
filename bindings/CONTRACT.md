<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Binding Conversion Contract

The FFI boundary is a genuine seam with four adapters (C++, Python, C#,
VB.NET). Every adapter satisfies the rules below; a divergence in any
adapter is a bug, caught by the drift net in §9 rather than by a
four-way diff.

## 1. Header snapshot

- Inline constant names come from the native struct for
  `0 <= i < JPL_MAX_CONSTANT_NAMES` (400); indices at or above the limit
  go through `jpl_get_constant_name`. The limit is the C constant, not a
  per-language literal.
- `len(constant_names) == constant_count`, and each entry equals a direct
  `constant_name(i)` call (pinned at indices 0, 399, 400, last).
- Snapshots are plain values, safe to use after the handle closes.

## 2. Date and state bridging

- Dates bridge as `[hi, lo]` pairs; states as `[x, y, z, vx, vy, vz]`
  sequences. Field order is fixed; units pass through unchanged
  (AU, AU/day, days; seconds and seconds/day for TT-TDB).

## 3. Context wrapping

- `EclipticToIcrs` is `frame_context_create` → `rotate_state` →
  `frame_context_destroy`, with destroy guaranteed (finally/dispose
  scope) even when rotation fails.
- The single-vector form wraps one `rotate_vector` call the same way; it
  must equal the state entry's position half bit for bit (same computer).

## 4. Solar-term rule

- A sun distance of `0.0` omits the solar term:
  `apply_aberration(s, v)` is bit-identical to
  `apply_aberration_full(s, v, 0.0)` in every language.

## 5. Round-trip rule

- `icrs_to_ecliptic(ecliptic_to_icrs(s, tt), tt)` reproduces `s` within
  1e-9 per component. Each language pins this on the same fixture.

## 6. Closed handles

| Language | Closed-handle behavior |
| -------- | ---------------------- |
| C++      | Moved-from/closed handle holds null; the C layer reports ARGUMENT, surfaced as `JplError` |
| Python   | Use after `close()` raises `JplError` with `ERROR_ARGUMENT` |
| C# / VB  | Use after dispose raises `ObjectDisposedException` via the safe-handle guard |

## 7. Input sizes

Undersized sequences are caller errors, surfaced per language idiom:
C++ is statically sized (`std::array`), C# / VB.NET throw
`ArgumentException`, Python raises `IndexError`. No adapter reads out of
bounds.

## 8. Error mapping

A non-OK `jpl_status` becomes `JplError` (C++), `JplError` carrying the
numeric status (Python), or `JplException` (C# / VB.NET). A null path
argument is rejected before the FFI call.

## 9. Drift net

Same fixture (`tests/data/de441_2000_2010.eph`, DE441, >400 names),
same expectations, four homes:

- C++: `bindings/cpp/tests/test_cpp_binding.cpp`
- Python: `bindings/python/tests/test_python_binding.py`
- C#: `bindings/csharp/tests/Program.cs`
- VB.NET: `bindings/vbnet/tests/Program.vb`

Each pins: overflow-name agreement (0/399/400/last), the §4
equivalence, the §5 round trip, and the §10 light-time solution fields.
If a new conversion is added to any adapter, add the same assertion to all
four.

## 10. Light-time solution

- `lighttime_solution(jd, target, center)` exposes the atomic
  `jpl_get_lighttime_solution()` value: `rel_state`, `observer_state`,
  `light_time_days`, and `emission`.
- `rel_state` and `light_time_days` equal `get_state_lighttime()`'s outputs
  bit for bit.
- `observer_state` is the center barycentric state at reception and equals
  a direct `get_state(jd, center, SSB)` within 1e-15 per component.
- `emission` is the TDB epoch used for `rel_state`;
  `|(jd - emission) - light_time_days| <= 1e-12` day.
- Every adapter raises/surfaces the same status mapping as its other
  light-time calls and leaves its result untouched on failure.
