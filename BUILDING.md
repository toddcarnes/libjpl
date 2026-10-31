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
| `LIBJPL_BUILD_SHARED` | `ON` | Build the shared library (`jpl.dll` / `libjpl.so`) in addition to the static library. |
| `LIBJPL_BUILD_TESTS` | `ON` | Build the offline test suite and test binaries. |
| `LIBJPL_BUILD_DOCS` | `OFF` | Build Doxygen API documentation. |
| `LIBJPL_FETCH_EPHEMERIDES` | `OFF` | Download DE440/DE440t validation binaries and register integration tests. |
| `LIBJPL_BUILD_CPP_BINDING` | `OFF` | Build C++ RAII wrapper tests (requires C++17 compiler). |
| `LIBJPL_BUILD_PYTHON_BINDING` | `OFF` | Build Python cffi binding tests (requires Python 3 + cffi). |
| `LIBJPL_BUILD_CSHARP_BINDING` | `OFF` | Build C# P/Invoke binding tests (requires .NET 8 SDK). |
| `LIBJPL_BUILD_VBNET_BINDING` | `OFF` | Build VB.NET P/Invoke binding tests (requires .NET 8 SDK). |

## Language Bindings

All language bindings require the shared library (`-DLIBJPL_BUILD_SHARED=ON`) and Python, C#, and VB.NET link against the shared library at runtime, while the C++ binding links against the static library. Each binding has its own CMake option (default `OFF`) and can be built/tested independently.

### C++ Binding (Header-only RAII Wrapper)

```sh
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CPP_BINDING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R jpl_cpp_binding --output-on-failure
```

Headers are installed to `${CMAKE_INSTALL_INCLUDEDIR}/jpl/` and provide `jpl::Ephemeris`, `jpl::JplError`, and `planet_position`, `moon_position`, and `tt_to_tdb` members of `jpl::Ephemeris`.

### Python Binding (cffi)

```sh
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_PYTHON_BINDING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R jpl_python_binding --output-on-failure
```

Requires Python 3 and `cffi` (`pip install cffi`). The test is skipped (not failed) if cffi is not importable.

### C# Binding (P/Invoke + SafeHandle)

```sh
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CSHARP_BINDING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R jpl_csharp_binding --output-on-failure
```

Requires .NET 8 SDK (`dotnet --version >= 8`). Provides `LibJpl.JplEphemeris : SafeHandle`, `LibJplApi.Open/OpenMmap`, and convenience functions. Test runs via a generated wrapper script that places `jpl.dll` / `libjpl.so` on the library path.

### VB.NET Binding (P/Invoke + SafeHandle)

```sh
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_VBNET_BINDING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R jpl_vbnet_binding --output-on-failure
```

Requires .NET 8 SDK. Mirrors the C# API: `LibJpl.JplEphemeris`, `LibJplApi.Open/OpenMmap`, enums `JplStatus`/`JplTarget`/`JplByteOrder`/`JplReferenceFrame`.

### Building All Bindings Together

```sh
cmake -S . -B build \
  -DLIBJPL_BUILD_SHARED=ON \
  -DLIBJPL_BUILD_CPP_BINDING=ON \
  -DLIBJPL_BUILD_PYTHON_BINDING=ON \
  -DLIBJPL_BUILD_CSHARP_BINDING=ON \
  -DLIBJPL_BUILD_VBNET_BINDING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Tests for missing toolchains (dotnet, Python, cffi, C++17) are silently absent, never failing.

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
- Warnings are treated at `/W4 /permissive-` with zero tolerance. CI builds opt in to fatal warnings via -DCMAKE_COMPILE_WARNING_AS_ERROR=ON.
