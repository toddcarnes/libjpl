<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Live documentation audit design

Date: 2026-09-26. This design covers an evidence-led audit and correction
of libjpl's current documentation, public Doxygen, and handwritten code
comments. It does not change library behavior or public declarations.

## Scope and authority

- Audit the live project guidance: `README.md`, `AGENTS.md`, `LICENSES.md`,
  `CHANGELOG.md` (current guidance and Unreleased entries),
  `docs/validation.md`, `docs/migration-two-part-dates.md`,
  `tests/data/PROVENANCE.md`, and `tests/data/numerical_reference.md`.
  Verify links, versions, prerequisites, examples, scientific statements,
  test registrations, and references to previous acceptance results.
- Audit Doxygen on all declarations in the three installed headers:
  `include/jpl.h`, `include/jpl_frames.h`, and
  `include/jpl_convenience.h`. Cross-check each of the 24 public functions
  against its implementation and, when needed, tests. Review types,
  enumerators, constants, and header-level notes for misleading claims.
- Audit comments in handwritten `src/jpl*.c`, `src/jpl_private.h`, build
  scripts, test sources, and test-support tools. Prioritize comments that
  describe behavior, file format, error handling, numerical accuracy,
  lifetime, threading, or a required workflow. Remove opaque task/fix IDs
  from live comments when the ID adds no explanation. Replace them with
  the current invariant and rationale; retain useful standards citations
  or a specific regression-test reference when they aid maintenance.
- Preserve dated records under `docs/{specs,plans}/` and
  `docs/reviews/` verbatim. Preserve past release entries in
  `CHANGELOG.md` as historical records. Correct live guidance that
  presents an older plan, observation, or pending gate as current.
- Do not edit vendored `src/erfa/` or pinned reference data. The
  existing uncommitted edits to `AGENTS.md` and `README.md` are user
  work for this audit: inspect them and retain their intent.

`docs/validation.md` describes current scientific contracts and
verification. The code and actual CMake/test registrations settle
implementation questions; historical reviews explain earlier decisions
but are not statements of current behavior.

## Review and correction method

1. Inventory active text and declarations. Keep a compact evidence list
   for each mismatch: location, current wording, counterexample in
   implementation/build/tests, and correction. Classify historical
   observations separately from live contracts.
2. For each public function, verify `@brief`, every applicable `@param`,
   `@return` and `@note`: accepted inputs; body and center codes; TT/TDB
   date pairs; units and frame; status/error precedence; missing-data and
   coverage behavior; exact aliasing; transactional outputs; and handle
   lifetime/thread-safety where applicable. Accuracy and solver tolerances
   must not be presented as interchangeable guarantees. Check supporting
   type, enum, and constant documentation against the same contract.
3. Review live Markdown and handwritten comments against actual code,
   CMake, installed artifacts, test registration, and source citations.
   Correct false or ambiguous present-tense statements surgically. Cut
   obsolete instructions rather than adding another disclaimer that leaves
   two conflicting instructions visible. Preserve substantive explanations
   of numerical or parsing invariants. Remediation IDs belong in commits
   and preserved review records, not as unexplained code-comment labels.
4. Keep all changes in documentation and comments. If a mismatch reveals
   an implementation defect, report it with a reproducible example and
   separate it from the documentation correction; do not silently change
   behavior. Summarize confirmed corrections and unresolved findings.

## Verification

- Review the diff for accidental changes to code, declarations, reference
  fixtures, and historical records; run `git diff --check`.
- Build Doxygen with `WARN_AS_ERROR=YES` and run `jpl_docs_symbols`,
  `jpl_docs_paths`, and `jpl_docs_only`. Generated symbol presence is a
  completeness check, not proof that explanations are correct.
- Run `jpl_examples` after README changes and `jpl_licenses` after changes
  to notices, Markdown or inventoried comments. Run focused functional
  tests when a corrected comment describes a numerical, parser, or API
  edge case; do not run the entire suite merely for prose edits.
- Record what was verified and any tests that could not run, without
  promoting historical acceptance results into current runs.

No additional public API, build option, runtime dependency, or ephemeris
format is introduced by this audit.
