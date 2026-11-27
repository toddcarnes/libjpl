<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Review follow-up resolution

- Date: 2026-09-25; base revision `05e9c6f`.
- Fix revisions: `d132b79` (F1-F7) and `0038224` (main-checkout
  license sweep correction).
- Work branch: `bugfix/review-followup-fixes`. This log records file-level
  fixes and observed tests, not a release claim.
- Approved design and implementation plan archived in the review history.

| ID | Change and regression | Evidence |
| --- | --- | --- |
| F1 | `CMakeLists.txt` compiles `jpl_testing` separately for the five tests that call private hooks. Installed `jpl` does not define `LIBJPL_TESTING`. `tests/cmake/check_archive_symbols.cmake` checks defined archive symbols for primary and tests-OFF child installs; registered in `tests/license_inventory.cmake`. | The baseline archive contained both leaked hooks; the new checker failed on `jpl_test_lighttime_with_budget`. Five hook-dependent tests 5/5; Windows and Linux `jpl_install` pass. |
| F2 | `tools/fixture_tool.c` replaces the Python mutator and in-script canonicalization; `tests/cmake/test_fixture_integrity.cmake` and `tests/cmake/check_numerical_reference.cmake` now invoke it. | All five mutation modes and canonical copies matched old Python outputs byte-for-byte before `tests/cmake/mutate_fixture.py` was removed. Four tool/integrity tests pass with `PATH=/nonexistent` on Linux. |
| F3 | `include/jpl_convenience.h` now describes one Moon light-time/aberration leg, with the multiple-leg warning scoped to callers who combine separate results. | Doxygen 1.17.0 docs build plus three docs checks pass; implementation still delegates once to `jpl_planet_position`. |
| F4 | `src/jpl.c` comment recognizes a TT-TDB span ending at record width as valid. | `jpl_tt_tdb` passes on Windows Debug/Release and native Linux Debug/Release; earlier real DE440t evidence belongs to the previous remediation, not a new real-tier run. |
| F5 | `src/jpl.c` compares selector dates as two-part epochs and clamps scalar candidate quotients before `size_t` conversion, including subinterval selection. `jpl_test_select_record` is private to the testing archive; `tests/test_api_errors.c` exercises far finite dates and transactional outputs. | Regression failed to link before the seam was implemented, then `jpl_api_errors` and related numerical/date/parser tests passed; native float-cast-overflow sanitizer run passed. |
| F6 | `tests/cmake/check_fixture.cmake` and the numerical-reference checker both canonicalize with the same binary-mode C tool; bare CR fails explicitly. Identity-list outputs have literal independent assertions in `tests/cmake/test_fixture_tool.cmake`. | New identity-list assertions failed against an intentionally broken output mode, then passed when correct output was restored. Fixture negative tests and CRLF/bare-CR controls pass. |
| F7 | Removed the unreachable `JPL_ERROR_NUMERIC` branch from `jpl_km_to_au` and its public `@return` contract; sibling velocity overflow handling remains. | `jpl_convenience` and `jpl_examples` pass Windows and Linux Debug/Release. |

## Acceptance

| Configuration | Observed result |
| --- | --- |
| Fresh Windows MSVC Debug and Release offline suites | **42/42** in each; no compiler warnings |
| Windows Doxygen `WARN_AS_ERROR=YES` | docs build; symbols/paths/docs-only **3/3** |
| Ubuntu WSL GCC 15.2 native filesystem Debug and Release | **42/42** in each; no compiler warnings; includes install/licenses/docs-only |
| Ubuntu native ASan/UBSan/float-cast-overflow | **40/40** applicable offline tests; install/docs-only tested unsanitized |
| Ubuntu native TSan threads/mmap | **2/2** |
| Ubuntu native no-Python-PATH fixture selection | **4/4** |

Ubuntu source was copied into `/home/todd/libjpl-review` before configuring
or building. Its `tools/fixture_tool.c` SHA-256 matched the source worktree
(`64ee5c44338a8e6f1345922e647de83629dc699c54722e99b86d5a382b4eba75`).
The first native Debug attempt timed out in `jpl_docs_only`; a focused
reproduction showed the missing `dot` executable. The maintainer approved
installing Graphviz in Ubuntu, after which full Debug and Release runs
passed. Linux Clang, Windows clang-cl, macOS, and opt-in real ephemeris
tests were not run for this follow-up.

## Main-checkout integration follow-up

The first merged-checkout run passed 40/42 tests. `jpl_licenses` and
`jpl_install` failed because the license inventory sweep traversed the
ignored `.worktrees/` directory containing the implementation worktree.
This was not visible when tests ran inside the linked worktree itself.
`tests/cmake/check_licenses.cmake` now excludes `.worktrees/`, and
`tests/cmake/test_licenses.cmake` constructs a positive nested-worktree
case that fails with the old checker. Both focused license/install tests
pass with `.worktrees/` still present in the main checkout. After the
second fast-forward to `0038224`, `build/merge-verify` on `main` passed
**42/42** Windows MSVC Debug tests with the linked worktree still present.
The updated native Ubuntu source copy also passed the full Debug and
Release suites, **42/42** in each configuration.
