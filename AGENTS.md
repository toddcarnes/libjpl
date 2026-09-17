Small, dependency-free C17 library for reading JPL Development Ephemeris
(DE) binaries. Static lib `jpl` + three public headers.

## Protocol deviations

If this guide's prerequisites aren't met (no git repo, missing
toolchain, conflicting instructions), stop and ask before working
around them — do not silently deviate. This applies to workflow
and infrastructure only; ordinary implementation judgment calls
stay with the implementer (record them in the commit message or
task ledger, don't stall on them).

## Layout

- `include/` — public API: `jpl.h` (core), `jpl_frames.h` (ICRS frames),
  `jpl_convenience.h` (planet/moon helpers). All public decls need Doxygen
  (`@brief/@param/@return/@note`); keep `WARN_AS_ERROR=YES` clean.
- `src/` — `jpl.c`, `jpl_frames.c`, `jpl_convenience.c`, internal
  `src/jpl_private.h` (shared constants/helpers, NOT installed),
  `src/erfa/` (vendored ERFA v2.0.1, BSD-3 — never modify sources here).
- `tests/` — one executable per `test_*.c`; shared mock builders live in
  `tests/jpl_test_util.h` (put_u32/put_f64/put_ptr, mock header, norm3).
- `docs/superpowers/{specs,plans}/` — design specs and implementation plans.

## Build & test

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- CMake 3.20+, `c_std_17`, `C_EXTENSIONS OFF`.
- MSVC: `/W4 /permissive-`, zero warnings. GCC/Clang:
  `-Wall -Wextra -Wpedantic`.
- MSVC is multi-config: always pass `-C Debug` (or `-C Release`) to ctest.
- Single-config generators default to Release (set in CMakeLists).
- `LIBJPL_BUILD_DOCS=OFF` by default; `ON` requires Doxygen (1.9+ known good).
- `JPLEPH` env var pointing at real DE files enables integration tests
  (`jpl_real_de441`, `jpl_all_ephemerides`); without it only 6 unit tests run.

## Conventions

- No outside dependencies: ERFA is vendored under `src/erfa/`, used via
  `file(GLOB)` + test-driver exclusion + version defines in CMakeLists.
- Always build: new features are unconditional; only docs/tests are options.
- Append, don't rewrite: public headers accumulate Doxygen + decls; use
  surgical edits so earlier comments survive.
- `jpl_apply_aberration` and frame helpers are pure math (no handle/I/O);
  I/O lives in `read_doubles` (native-order fast path) and the open/clone
  paths. Same `jpl_ephemeris*` must not be shared across threads on the
  FILE path — use `jpl_ephemeris_clone()` per thread or `jpl_open_mmap()`.
- Physics constants come from `src/jpl_private.h`, never literals.
- Shell: agent-run commands execute under PowerShell 5.1, so keep them
  5.1-compatible — chain with `;` (no `&&`/`||`), quote paths with
  spaces, use `workdir` instead of `cd`. (The user shell is pwsh 7.6.6;
  7-only syntax must not appear in commands agents run.)
