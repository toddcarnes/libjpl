<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Migrating from `jpl_get_header` to the header accessors

`jpl_get_header()` and the public `jpl_header` field layout were removed in
0.12.0. `jpl_header` is now opaque; use the grouped accessors on the
ephemeris handle.

| Old field | New call |
| --- | --- |
| `titles` | `jpl_header_titles(e, titles)` |
| `start_jd`, `end_jd`, `step_days` | `jpl_header_coverage(e, &start, &end, &step)` |
| `constant_count`, `record_double_count`, `byte_order` | `jpl_header_shape(e, &count, &width, &order)` |
| `ephemeris_number`, `reference_frame` | `jpl_header_identity(e, &denum, &frame)` |
| `astronomical_unit_km`, `earth_moon_mass_ratio`, `has_tt_tdb` | `jpl_header_scalars(e, &au, &emrat, &has)` |
| `pointers[i]` | `jpl_header_pointer(e, i, &pointer)` |
| `libration_pointer` | `jpl_header_libration_pointer(e, &pointer)` |
| `tt_tdb_pointer` | `jpl_header_tt_tdb_pointer(e, &pointer)` |
| `constant_names[i]` | `jpl_get_constant_name(e, i, name)` |

Every accessor returns `JPL_ERROR_ARGUMENT` for NULL outputs and leaves the
outputs untouched on failure. `jpl_header_pointer` also rejects slots outside
`0..JPL_TARGET_COUNT-1`.

The binding adapters keep their existing public `Header` classes/properties;
only their internals changed. `bindings/CONTRACT.md` records the accessor
rule.
