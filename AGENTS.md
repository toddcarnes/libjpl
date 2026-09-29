<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

Small C17 library for reading JPL Development Ephemeris (DE) binaries.
Bundles ERFA v2.0.1; no separate third-party library installation is
required. Static lib `jpl` + three public headers.

## Protocol deviations

If this guide's prerequisites aren't met (no git repo, missing
toolchain, conflicting instructions), stop and ask before working
around them — do not silently deviate. This applies to workflow
and infrastructure only; ordinary implementation judgment calls
stay with the implementer (record them in the commit message or
task ledger, don't stall on them).

## Merging into main

Merge a branch into `main` only with explicit maintainer approval. Use
`git merge --no-ff <branch>` to create a merge commit and preserve branch
history; never fast-forward, rebase, or squash the branch into `main`.
Verify the merged result before removing the branch or its worktree.

## Layout

- `include/` — public API: `jpl.h` (core), `jpl_frames.h` (ICRS frames),
  `jpl_convenience.h` (planet/moon helpers). Document all public declarations;
  functions need Doxygen `@brief`, each `@param`, applicable `@return` and
  `@note`. Keep `WARN_AS_ERROR=YES` clean.
- `src/` — `jpl.c`, `jpl_date.c`, `jpl_frames.c`, `jpl_convenience.c`,
  `jpl_private.h` (internal, NOT installed),
  `src/erfa/` (vendored ERFA v2.0.1, BSD-3 — never modify sources here).
- `tests/` — `test_*.c` runners (some also have fault-injection variants);
  shared mock builders live in `tests/jpl_test_util.h`.
- `docs/superpowers/{specs,plans}/` — design specs and implementation plans.

## Current contracts

- [Validation notes](docs/validation.md) record current scientific contracts,
  platform support, and verification procedures.
- Public and in-memory Julian dates use `double[2]`; preserve both parts
  through arithmetic and ERFA calls. Never split an already-rounded scalar
  to create a two-part input. Keep standard DE on-disk scalar dates and
  promote them to `{scalar, 0.0}` when read.

## Build & test

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- CMake 3.20+; library targets use `c_std_17` with `C_EXTENSIONS OFF`.
- MSVC: `/W4 /permissive-`, zero warnings. GCC/Clang:
  `-Wall -Wextra -Wpedantic`.
- MSVC is multi-config: always pass `-C Debug` (or `-C Release`) to ctest.
- Single-config generators default to Release (set in `CMakeLists.txt`).
- `LIBJPL_BUILD_TESTS=ON` by default; the offline suite needs no network or
  Python. `LIBJPL_BUILD_DOCS=OFF` by default; docs checks need Doxygen.
  Linux docs checks also need Graphviz `dot` with the tested Doxygen setup.
- `LIBJPL_FETCH_EPHEMERIDES=OFF` by default. `ON` enables the DE440/DE440t
  fetch/reference tier; `JPLEPH` and `JPL_TESTPO` only redirect sources in
  that tier. The local sweep registers only when `JPLEPH` is set. Check
  `ctest --test-dir build -N` for actual registrations.
- With docs ON, `jpl_docs_symbols` and `jpl_docs_paths` register even with tests
  OFF. With tests ON, `jpl_docs_only` verifies that matrix, and
  `jpl_examples` runs README examples.

## Conventions

- ERFA sources under `src/erfa/` are built via `file(GLOB)` with
  test-driver exclusion and version defines in CMake.
- Library features build unconditionally; docs and tests have options.
  Test-only `jpl_testing` and `jpl_fixture_tool` are not installed.
- Append, don't rewrite: public headers accumulate Doxygen + decls; use
  surgical edits so earlier comments survive.
- `jpl_apply_aberration` and frame helpers are pure math (no handle/I/O);
  I/O lives in `read_doubles` (native-order fast path) and the open/clone
  paths. Same `jpl_ephemeris*` must not be shared across threads on the
  FILE path — use `jpl_ephemeris_clone()` per thread or `jpl_open_mmap()`.
- Physics constants come from `src/jpl_private.h`, never literals.
- Agent commands run under PowerShell 5.1: avoid `&&`/`||`, use
  `; if ($?) { ... }` for dependent commands, quote paths with spaces,
  and use `workdir` instead of `cd`.
- **License headers**: Every source file created must carry the MIT license
  header with `Copyright (c) 2026 Todd Carnes` and `SPDX-License-Identifier: MIT`
  in the file's native comment syntax (C `/* */`, `#` for scripts, `<!-- -->` for
  XML/HTML, `//` for C#). This is enforced by the `jpl_licenses` test via
  `tests/license_inventory.cmake`. New files must be added to that inventory.
