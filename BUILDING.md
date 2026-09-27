<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Building libjpl

## Prerequisites

- CMake 3.20+
- A C17 compiler: MSVC 19.30+, GCC 11+, or Clang 13+
- No external runtime dependencies (ERFA v2.0.1 is vendored under `src/erfa/`)
- For the test suite: a POSIX threads library on Linux (provided by the toolchain)

## Standard Build

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`--config`/`-C` is required on multi-config generators (MSVC, Ninja Multi-Config,
Xcode); omit on single-config Makefiles/Ninja, which default to Release.

### Build Options

| Option | Default | Description |
|---|---|---|
| `LIBJPL_PIC` | `ON` | Build the static library with position-independent code (needed to link into a shared object). |
| `LIBJPL_BUILD_TESTS` | `ON` | Build the offline test suite and test binaries. |
| `LIBJPL_BUILD_DOCS` | `OFF` | Build Doxygen API documentation. |
| `LIBJPL_FETCH_EPHEMERIDES` | `OFF` | Download DE440/DE440t validation binaries and register integration tests. |

## Building Documentation

Documentation requires Doxygen. On Linux, Graphviz `dot` is also needed for
call graphs and the `jpl_docs_paths` path-with-spaces check.

```sh
cmake -S . -B build -DLIBJPL_BUILD_DOCS=ON
cmake --build build --config Debug --target docs
```

### Platform Notes for Docs

- **Windows:** Install Doxygen via `choco install doxygen` or download from
  doxygen.nl. Graphviz (`choco install graphviz`) is optional — call graphs are
  omitted if `dot` is not found, but the Doxygen run itself succeeds without it.
- **macOS:** `brew install doxygen graphviz`
- **Linux:** `apt-get install doxygen graphviz` (or the equivalent for your distro)

Generated output:

- HTML: `build/docs/html/index.html`
- XML: `build/docs/xml/` (consumed by the `jpl_docs_symbols` CTest check)

## Fetching Ephemeris Data (Optional)

Integration tests against real DE440/DE440t binaries require a ~210 MB download:

```sh
cmake -S . -B build -DLIBJPL_FETCH_EPHEMERIDES=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The offline suite (`LIBJPL_FETCH_EPHEMERIDES=OFF`, the default) runs without
network access or Python.

## MSVC Notes

- Use the Visual Studio generator (multi-config): always pass `-C Debug` or
  `-C Release` to `ctest`.
- Warnings are treated at `/W4 /permissive-` with zero tolerance.
