<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

Small C17 library for reading JPL Development Ephemeris (DE) binaries.
Bundles ERFA v2.0.1; no separate third-party library installation is
required. Builds static (`jpl_static`) and shared (`jpl_shared`) libraries,
public C headers (`jpl.h`, `jpl_frames.h`, `jpl_convenience.h`, generated
`jpl_export.h`), and optional bindings for C++, Python, C#, and VB.NET.

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

## Branch workflow

All changes must be made on a working branch, not directly on `main`:

1. **Create and switch** before any edits:
   ```sh
   git checkout -b <branch-name>
   ```

2. **Verify branch** before committing:
   ```sh
   git branch  # confirm * <branch-name>
   ```

3. **Commit to the branch** (not main):
   ```sh
   git add ...
   git commit -m "..."
   ```

4. **Test on the branch** before merge.

5. **Merge with `--no-ff`** to `main` only after verification:
   ```sh
   git checkout main
   git merge --no-ff <branch-name>
   ```

6. **Tag and delete** branch after successful merge:
   ```sh
   git tag -a vX.Y.Z -m "Release ..."
   git branch -d <branch-name>
   ```

Never commit directly to `main` without a branch. The `git merge --no-ff`
requirement in the section above assumes the work was done on a branch.

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
- `docs/specs/` — design specs.
- `docs/plans/` — implementation plans.

## Current contracts

- [Validation notes](docs/validation.md) record current scientific contracts,
  platform support, and verification procedures.
- Public Julian dates and states use `jpl_date {hi, lo}` and `jpl_state`
  structs; internal arithmetic uses normalized `double[2]` pairs and
  `double[6]` arrays. Preserve both date parts through arithmetic and ERFA
  calls; construct two-part dates directly rather than splitting an
  already-rounded scalar. Keep standard DE on-disk scalar dates and promote
  them to `{scalar, 0.0}` when read.

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

## Agent skills

### Issue tracker

Issues live in GitHub Issues for `toddcarnes/libjpl`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five-role vocabulary (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context layout (root `GLOSSARY.md` + `docs/adr/`). See `docs/agents/domain.md`.
