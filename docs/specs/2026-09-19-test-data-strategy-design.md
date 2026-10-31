# Public-Release Test Data Strategy Design

## Purpose

Make a fresh public clone build and test green with no network and no
large files, while keeping full JPL reference validation available as
an explicit opt-in. Binary ephemerides (~4.5 GB locally, 2.6 GB for
DE441 alone) can never live in the repo; testpo files (~75 MB total)
are deliberately not committed either (generated JPL artifacts, not
source).

## Tiers

- **Always-on, no network (PR gate):** the 6 existing unit suites plus
  a new `jpl_fixture_tests` suite opening committed
  `tests/data/de441_2000_2010.eph` (~931 KB sub-ephemeris sliced from
  DE441.eph covering years 2000-2010 via
  `sub_eph DE441.eph <out> 2000 2010`; JD 2451536.5-2455216.5) and
  cross-checking the 121 in-slice lines committed alongside as
  `tests/data/testpo_441_slice.txt`. The suite asserts fixture
  provenance (DENUM 441, exact coverage bounds) so a regenerated
  fixture cannot silently drift.
- **Opt-in download (`LIBJPL_FETCH_EPHEMERIDES`, default OFF):** when
  ON, CMake fetches DE440 + DE440t binaries plus testpo.440/440t from
  JPL (`https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/` and
  `.../de440t/`, filenames `linux_p1550p2650.440[t]` plus co-located
  `testpo.440[t]`), verifies them per the manifest, and registers the
  integration + full testpo suites. Plain `cmake -S . -B build` never
  touches the network. DE440/DE440t (~205 MB) cover every code path
  including T#17; the 31-file local sweep stays a maintainer activity.
- **Local override:** `JPLEPH`/`JPL_TESTPO` env dirs redirect download
  sources but no longer auto-register suites on their own. The CMake
  option is the single switch; env vars just pick the source. Current
  maintainer workflow (option ON + local env dirs) keeps working.

## Manifest and failure behavior

- Committed `tests/data/MANIFEST.txt` records per download file: exact
  URL, expected byte size in bytes (captured from the FTP listing at
  implementation time; the 97.5M/107.5M figures are display-rounded),
  testpo SHA-256 where applicable (`testpo.440: 45084a7f...`,
  `testpo.440t: 9801f019...`, computed 2026-09 from local copies), the
  fixture-slice provenance (`sub_eph` invocation, source, byte size),
  and the date verified.
- Binaries are verified by exact byte size plus content checks the
  library already performs (DENUM, coverage span, kernel size) with the
  testpo suite itself going green as final proof. Local `checksums.txt`
  hashes describe differently-spanned local copies (e.g. 102,272,352
  bytes for DE440.eph vs the server's 1550-2650 span) and cannot verify
  downloads, so no binary hashes are pinned.
- Any download/verification failure (no network, 404, size or hash
  mismatch) fails configure with an error naming the file, expected vs
  actual, and the URL. Never a silent skip or half-registered suite. A
  JPL re-cut is fixed by a single manifest commit.

## Non-goals

- No testpo files committed beyond the 121-line slice; no binaries
  committed beyond the 931 KB slice.
- No change to `JPL_TESTPO` absolute-path default handling beyond
  gating it behind the fetch option.
