<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# 2026-11-20 Review Remediation Implementation Plan

> **For agentic workers:** use checkboxes (`- [ ]` / `- [x]`) for tracking.
> Work one workstream at a time in order, stopping at the end of each
> workstream to report progress and wait for maintainer permission before
> starting the next.

**Goal:** Resolve all 13 findings from the 2026-11-20 codebase and
documentation review, including the solar light-deflection no-op in
`jpl_apparent_ecliptic_position()`, C#/VB.NET P/Invoke ABI and
`jpl_status_string` memory-safety bugs, C++/Python `constant_names`
out-of-bounds reads, C17 UB in `src/jpl.c`, broken binding examples,
CMake/packaging edge cases, and stale/corrupted documentation.

**Branch:** `fix/2026-11-20-review-remediation` (forked from `main` @ `3a3c4d9`).

**Local data:** `DE441.eph` is available locally at `D:\ephem\JPL\bin`
(pass via `$env:JPLEPH = "D:\ephem\JPL\bin"` when running Horizons checks).

## Settled Decisions

- **D1 (`docs/superpowers/`):** Remove the duplicate `docs/superpowers/`
  tree and its exemption entry in `tests/license_inventory.cmake` (and
  `tests/cmake/check_licenses.cmake` if referenced).
- **D2 (`header.constant_names` when `constant_count > 400`):** Populate
  all `constant_count` names via `jpl_get_constant_name()` in C++ and
  Python (both return dynamic collections).
- **D3 (`jpl_frame_context_rotate_*` `const`):** Change
  `jpl_frame_context *context` to `const jpl_frame_context *context` in
  `include/jpl_frames.h` and `src/jpl_frames.c`.

## Global Constraints

- Do not modify `src/erfa/`; use ERFA only through internal library code.
- Library targets keep `c_std_17` with `C_EXTENSIONS OFF`.
- Zero compiler warnings: MSVC `/W4 /permissive-`, GCC/Clang
  `-Wall -Wextra -Wpedantic` (`-DCMAKE_COMPILE_WARNING_AS_ERROR=ON`).
- Public headers accumulate Doxygen; surgical edits only (`WARN_AS_ERROR=YES`).
- Physics constants come from `src/jpl_private.h`, never literals.
- Every new non-exempt source file carries the MIT header with
  `Copyright (c) 2026 Todd Carnes` / `SPDX-License-Identifier: MIT` and is
  registered in `tests/license_inventory.cmake`.
- Commands run under PowerShell 5.1: no `&&`/`||`, use `; if ($?) { ... }`,
  always pass `-C Debug`/`-C Release` to `ctest` on MSVC.
- Merge into `main` only with explicit maintainer approval using
  `git merge --no-ff fix/2026-11-20-review-remediation`.

---

## Workstream 0 — Branch & Plan Capture
- [x] Create and switch to `fix/2026-11-20-review-remediation`.
- [x] Save this plan to `docs/plans/2026-11-20-review-remediation.md`.
- [x] Commit plan on the working branch.

---

## Workstream 1 — Core C Library Correctness & UB *(skill: `tdd`)*
- [x] **1.1 Solar light deflection (`src/jpl_convenience.c`, `tests/test_convenience.c`)**
  - Red: Add near-solar-limb (~1° elongation) regression test in `tests/test_convenience.c` comparing `jpl_apparent_ecliptic_position()` against independent `eraLd()` with `q = Sun->target`, `e = Sun->observer`.
  - Green: Fix `apparent_apply_deflection()` and `apparent_position_at_tt()` to pass `q` (`sun_to_target` normalized) and `to_sun` (negated to `e`), and update the doc comment in `src/jpl_convenience.c`.
  - Verify `jpl_convenience`, `jpl_numeric`, and `jpl_horizons_*` against `D:\ephem\JPL\bin\DE441.eph`.
- [x] **1.2 C17 UB & strict-aliasing cleanup (`src/jpl.c`)**
  - Replace `(&state->x)[i]` in `jpl_apply_aberration()` with `pos[i]`.
  - Replace `(double *)state` in `jpl_get_state()` (nutations/librations) with `local[6]` + `jpl_array_to_state(local, state)`.
  - Add portable `#else` byte-swap fallback in `read_f64()`.
- [x] **1.3 `jpl_frame_context` `const`-correctness & docs (`include/jpl_frames.h`, `src/jpl_frames.c`)**
  - Apply D3 (`const jpl_frame_context *context`) and document rotation direction (mean ecliptic-of-date -> ICRS).
- [x] **1.4 Core hygiene (`src/jpl_private.h`, `src/jpl_convenience.c`)**
  - Remove duplicate `JPL_HDR_TITLES_LEN` / `JPL_HDR_TEXT_LEN` defines in `src/jpl_private.h`.
  - Deduplicate `eff_center != JPL_SUN` branches and remove unused `lt` in `jpl_planet_position()`.
  - Replace `180.0 / acos(-1.0)` with `ERFA_DR2D` in `apparent_project_ecliptic()`.

---

## Workstream 2 — CMake & Packaging
- [x] **2.1 Export header hygiene (`cmake/jpl_export.h.in`)**
  - Remove `#cmakedefine JPL_BUILDING_SHARED` so `jpl_export.h` relies only on `target_compile_definitions(jpl_shared PRIVATE JPL_BUILDING_SHARED)`.
- [x] **2.2 `package-sdk` without Doxygen (`CMakeLists.txt`)**
  - Guard the `${JPL_SDK_DOCS_WORK}/html` copy on `DOXYGEN_FOUND`.

---

## Workstream 3 — Language Bindings & Examples *(skill: `tdd`)*
- [x] **3.1 C# & VB.NET P/Invoke ABI & `jpl_status_string` (`bindings/csharp/`, `bindings/vbnet/`)**
  - Red: Add `LibJplApi.StatusString(...)` assertions in `Program.cs` and `Program.vb`.
  - Green: Change all 11 `JplDate`/`JplDateNative` inputs to `in JplDate` / `ByRef ... As JplDateNative`; change `jpl_status_string` return type to `IntPtr` and decode via `Marshal.PtrToStringUTF8(...)`.
- [x] **3.2 C++ binding (`bindings/cpp/`)**
  - Red: Add test reading `header().constant_names` on `de441_2000_2010.eph` (`constant_count > 400`).
  - Green: Populate all `constant_count` names via `jpl_get_constant_name()` (D2); use `Header::Pointer` for `libration_pointer`/`tt_tdb_pointer`; use `JPL_CONSTANT_NAME_LENGTH + 1`.
- [x] **3.3 Python binding (`bindings/python/`)**
  - Red: Add tests for failed `Ephemeris("nonexistent")` + GC, `header.constant_names` with `constant_count > 400`, `hdr = eph.header` lifetime after close/GC, and top-level exports.
  - Green: Initialize `self._handle = None` before `jpl_open`; prevent UAF in `JplHeader`; populate all `constant_count` names via `jpl_get_constant_name()` (D2); export full public surface in `__init__.py` and remove duplicate `'JplError'` in `__all__`.
- [x] **3.4 Fix all four binding examples**
  - Fix and smoke-test `bindings/cpp/examples/planet_positions.cpp`, `bindings/python/examples/planet_positions.py`, `bindings/csharp/examples/PlanetPositions.cs`, and `bindings/vbnet/examples/PlanetPositions.vb` against `tests/data/de441_2000_2010.eph`.

---

## Workstream 4 — Documentation, Doxygen Inventory & Governance *(skill: `writing-for-agents`)*
- [ ] **4.1 Doxygen & public symbol inventory (`docs/api.dox`, `tests/public_symbols.txt`)**
  - Add `jpl_date`, `jpl_state`, `jpl_frame_context`, and all 5 `jpl_frame_context_*` functions to `tests/public_symbols.txt` and `docs/api.dox`.
- [ ] **4.2 Fix corrupted characters & duplicate entries (`docs/validation.md`, `CHANGELOG.md`)**
  - Fix literal `\b` in `docs/validation.md:199` and `CHANGELOG.md`.
  - Remove duplicate license-sweep bullets from `0.4.0`/`0.3.0`/`0.2.0` in `CHANGELOG.md`; fix `0.5.0` shared runtime install path; add `[Unreleased]` entries for all changes on this branch.
- [ ] **4.3 Align stale docs (`AGENTS.md`, `README.md`, `docs/reviews/2026-10-31-review.md`, D1)**
  - Update `AGENTS.md` using `writing-for-agents` guidelines.
  - Fix `README.md:135` (`jpl::Ephemeris::planet_position`).
  - Fix `docs/reviews/2026-10-31-review.md:1-3` comment syntax (`<!-- ... -->`).
  - Remove `docs/superpowers/` and its exemption in `tests/license_inventory.cmake` / `tests/cmake/check_licenses.cmake` (D1).

---

## Workstream 5 — Full Verification & Pre-Merge Review *(skill: `code-review`)*
- [ ] Windows MSVC Debug & Release with `-DCMAKE_COMPILE_WARNING_AS_ERROR=ON` (offline suite + `jpl_licenses` + `jpl_install` + `jpl_examples` + `jpl_docs_only`).
- [ ] Horizons suite with `$env:JPLEPH = "D:\ephem\JPL\bin"`.
- [ ] Docs build (`-DLIBJPL_BUILD_DOCS=ON`): `docs`, `jpl_docs_symbols`, `jpl_docs_paths`.
- [ ] All bindings enabled (`-DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_{CPP,PYTHON,CSHARP,VBNET}_BINDING=ON`) in Debug & Release.
- [ ] WSL Linux GCC/Clang + ASan/UBSan + Linux binding tests (if available).
- [ ] Record observed verification results in `docs/validation.md`.
- [ ] Run `code-review` skill against `main` and report before requesting merge approval.
