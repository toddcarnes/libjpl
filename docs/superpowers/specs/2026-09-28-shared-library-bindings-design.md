<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Shared library and language bindings

## Goal and boundaries

Ship `libjpl` as both a static archive and a versioned shared library
under one library name, installed into separate `static/` and `shared/`
directories, and supply in-repo bindings for C++, Python (cffi),
C# (P/Invoke), and Visual Basic .NET (direct P/Invoke port). The C API
and its scientific contracts do not change: no new `jpl_*` C symbols,
no ERFA exposure, no physics changes. Consumers choose static or shared
at link time; bindings target the shared library at runtime.

Out of scope: new ephemeris features, new C API functions, NuGet/PyPI
publishing automation, and any modification of `src/erfa/`.

## Decisions already approved

- Two CMake targets, one output name (Option A): `jpl_static` and
  `jpl_shared` both set `OUTPUT_NAME "jpl"`, installed to
  `${CMAKE_INSTALL_LIBDIR}/static` (archive) and
  `${CMAKE_INSTALL_LIBDIR}/shared` (shared object) respectively, with
  Windows DLLs under `${CMAKE_INSTALL_BINDIR}/shared`.
- Symbol visibility via a generated `jpl_export.h` (from
  `cmake/jpl_export.h.in` through `configure_file`), included by the
  three public headers. `JPL_API` prefixes every public function
  declaration. No build-system macros leak into installed headers beyond
  the generated export header itself.
- Bindings live in this repo under `bindings/{cpp,python,csharp,vbnet}/`.
- Python via cffi, C# via P/Invoke with `SafeHandle`, C++ via
  header-only RAII wrapper, VB.NET as a direct P/Invoke port of C#.

## Public contract

- Library filenames stay `jpl.lib` / `libjpl.a` (static) and
  `jpl.dll` / `libjpl.so` / `libjpl.dylib` (shared). Only the install
  directories differ.
- `jpl::jpl` remains a valid CMake alias and keeps meaning the static
  library. New aliases `jpl::jpl_static` and `jpl::jpl_shared` select
  explicitly. The installed `libjplConfig.cmake` exports all three.
- Installed headers become four files: `jpl.h`, `jpl_frames.h`,
  `jpl_convenience.h`, plus generated `jpl_export.h`.
- Linux/macOS shared library carries `VERSION ${PROJECT_VERSION}` and
  `SOVERSION ${PROJECT_VERSION_MAJOR}`. Windows DLL carries a
  `VERSIONINFO` resource stamped with the project version.
- Threading, error-status, two-part-date, frame-gating, and modern-AU
  contracts are unchanged. Bindings surface the same status codes and
  transactional-output guarantees with idiomatic error handling per
  language (exceptions in C++/C#/VB.NET/Python).

## Build layout

- `cmake/jpl_export.h.in` is the template; the generated header lands in
  the build tree and installs to `${CMAKE_INSTALL_INCLUDEDIR}`.
- A `cmake/jpl_version.rc.in` template stamps the Windows DLL only.
- Each binding is gated by its own CMake option so a missing toolchain
  (dotnet, Python/cffi) never breaks the default C build:
  `LIBJPL_BUILD_SHARED` (default ON), `LIBJPL_BUILD_CPP_BINDING`,
  `LIBJPL_BUILD_PYTHON_BINDING`, `LIBJPL_BUILD_CSHARP_BINDING`,
  `LIBJPL_BUILD_VBNET_BINDING` (binding options default OFF).
- `tests/cmake/test_install.cmake` and
  `tests/cmake/check_archive_symbols.cmake` learn the `static/` and
  `shared/` layout; the archive-symbol check keeps asserting that no
  `jpl_test_*` symbols ship, now against the static archive path.
- `package-sdk-zip` output gains the shared binary, the generated
  export header, the C++ wrapper headers, and-built binding artifacts
  present at package time.

## Binding contracts

- C++ (`bindings/cpp/include/jpl/*.hpp`): header-only, C++17, RAII
  `jpl::Ephemeris` owning `jpl_ephemeris*`, move-only, exceptions on
  non-`JPL_OK`. No separate compilation; install the headers.
- Python (`bindings/python/`): cffi `ffi.cdef()` covering the full
  public C API, `Ephemeris` context-manager class, `JplError` exception
  carrying the `jpl_status` value. `pyproject.toml` declares the cffi
  dependency; tests run only when cffi is importable.
- C# (`bindings/csharp/`): `DllImport("jpl")` with
  `CallingConvention.Cdecl`, `SafeHandle`-derived `JplEphemeris`,
  enums mirroring `jpl_status`/`jpl_target`/`jpl_reference_frame`,
  structs with explicit layout for `jpl_header`-adjacent plain data.
  Built and tested via `dotnet` when available, otherwise skipped.
- VB.NET (`bindings/vbnet/`): same P/Invoke surface in VB syntax,
  kept in sync with the C# declarations by side-by-side review.

## Verification

- Default configure plus full `ctest` passes on the existing suite with
  zero warnings (`/W4 /permissive-` on MSVC,
  `-Wall -Wextra -Wpedantic` elsewhere, `WARN_AS_ERROR=YES` for docs).
- New CTest cases: shared-library consumer link-and-run, export-header
  presence in install, C++ wrapper compile-and-run, Python cffi tests
  (skipped without cffi), C#/VB.NET tests (skipped without dotnet).
- The install driver verifies both `static/` and `shared/` artifacts
  and both new aliases.
