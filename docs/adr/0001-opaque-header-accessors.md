<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# ADR 0001: Opaque `jpl_header` accessors

- **Status:** accepted (2026-11-27)
- **Context:** The 3320-byte `jpl_header` public value struct was mirrored by
  hand in C equality code and in all four binding adapters. Adding one field
  required synchronized edits in the parser, `headers_equal`, C++/Python/C#/
  VB mirrors, and the managed layout tests. The drift net pinned values, not
  the field set.
- **Decision:** Make `jpl_header` opaque and expose grouped accessors:
  `jpl_header_titles`, `jpl_header_coverage`, `jpl_header_shape`,
  `jpl_header_identity`, `jpl_header_scalars`, `jpl_header_pointer`,
  `jpl_header_libration_pointer`, and `jpl_header_tt_tdb_pointer`.
  `jpl_get_header` is deleted. The four bindings build their existing public
  `Header` classes from accessor calls and no longer mirror the native layout.
- **Alternatives considered:**
  - *Versioned snapshot struct:* smaller migration, but keeps the field-list
    duplication and only detects drift at runtime. Retained as the documented
    fallback if the accessor migration had failed its gate.
  - *Keep the struct unchanged:* rejected; it does not address the replicated
    layout.
- **Gate outcome:** The binding public `Header` classes/properties were kept
  unchanged and built from accessors; the C test migration was a mechanical
  field-access replacement. The opaque path was selected.
- **Consequences:** The C header layout is private. Bindings are insulated from
  field additions, and `bindings/CONTRACT.md` records the accessor rule.
  Consumers use the accessor set or `jpl_get_constant_name`.
