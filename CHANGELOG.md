# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- TT-TDB dataset support: parse the header pointer, report dataset presence, and expose the dataset from time-segmented ephemerides with record-geometry span validation.
- Committed DE441 2000-2010 fixture with manifest, validated with zero network.
- Opt-in JPL download tier: network test data gated behind a fetch option, with integration suites wired to fetched data.

### Changed
- Body codes Earth=3 / EMB=13 per JPL PLEPH convention (**BREAKING CHANGE**: raw-integer callers swap 3/13).
- Test harness: single testpo runner with the manifest as single source, fixture provenance checks and SRS regression coverage; testpo suites registered for time-segmented DE variants.

### Fixed
- Light-time iteration uses the center epoch; observer velocity uses the full aberration term at the reception epoch.
- SRS relativistic constant corrected.
- Overflow and record-offset guard wraps closed, including the TT-TDB tail parse.
- Mmap reads lock-free and shareable (FILE path cached per handle); POSIX same-fd size recheck.
- Explicit test checks, off-J2000 golden vector, scoped libration envelope, Release-safe mocks, ASCII-only build comments.
