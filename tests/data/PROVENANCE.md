<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# libjpl fixture provenance

Committed artifacts (hashes pinned in `tests/data/MANIFEST.txt`):

- `tests/data/de441_2000_2010.eph`: 952848 bytes, SHA-256
  `4478771dcf58f031c823a8a6ba721a934c1d2fbadad2020a3d446310617b7b01`
  (= 8144 * 117 records). No DENUM/coverage claim here can substitute
  for the runner's header assertions (DENUM 441, coverage
  2451536.5-2455216.5, no TT-TDB table).
- `tests/data/testpo_441_slice.txt`: 121 seven-token `testpo.441` data
  rows, first-of-month dates from `2000.01.01` (JD 2451544.5) to
  `2010.01.01` (JD 2455197.5). Canonical LF SHA-256
  `15b15ef811df9d2d7e054f6ebcdefae33c0c89467ef9d2fa5bd9cc386d8e93f4`
  (raw checkout bytes differ under CRLF; only CRLF-to-LF normalization
  is applied before hashing). Checkout line endings may differ by
  platform; the git blob is LF.
- `tests/data/fixture_rows.txt`: ordered identity list, one
  `JD target center component` quadruple per slice row in file order
  (121 lines). Canonical LF SHA-256
  `540055b7352d4ef97ae67345e140cbf56b7f121fa6f8505c52a471739dd5e731`.

What each check establishes (and does not):

- `tests/cmake/check_fixture.cmake` (`jpl_fixture_integrity`): data
  integrity of the three committed artifacts against the manifest
  hashes. A matching hash pins the present bytes; it does not prove
  where they came from.
- `tests/test_fixture.c` (`jpl_fixture`): numerical identity -- every
  one of the 121 slice rows interpolates to its reference coordinate
  within the shared testpo tolerance, and every parsed row's
  decimal-JD/target/center/component identity matches
  `fixture_rows.txt` row for row. Count (`parsed == tested == 121`)
  and endpoint dates alone do NOT prove this; the ordered identity
  comparison plus the pinned identity-list hash do. The negative suite
  proves it: 119 duplicated interior rows (endpoints and count kept)
  fail the identity/text hashes.
- Upstream provenance (that the bytes are the true JPL DE441/testpo.441
  artefacts sliced with the stated tool) comes only from the source
  evidence below, not from any hash or row count.

Source evidence (recovered 2026-09-18, files not modified):

- Maintainer-local full ephemeris `D:\ephem\jpl\bin\DE441.eph`:
  2788676624 bytes, SHA-256
  `4b0f478bff190136f3457a75f085b4e1668024f1d49a051200bdc0a2aebd72d2`,
  matching the maintainer `checksums.txt` entry for DE441.eph
  (Size 2788676624; SHA256
  `4B0F478BFF190136F3457A75F085B4E1668024F1D49A051200BDC0A2AEBD72D2`).
- Maintainer-local reference `D:\ephem\jpl\testpo\testpo.441`:
  23704828 bytes, SHA-256
  `e60c08ced7741a07d4dfe89b124afb4b1648bd1e3d0eb68dd9a79bf6990bb9c2`,
  LF-only, 364695 lines, header line
  `DE-0441LE-0441 ... JD -3100015.50 to ... JD 8000016.50`,
  `EOT` after the 7-line header.
- Slicing tool `D:\ephem\jpl\tools\sub_eph.exe` (usage:
  `sub_eph <large-binary> <sub-ephemeris> <start-year> <end-year>`):
  189952 bytes, SHA-256
  `5f1ff73d91a4614e251e79b0f0f813578cdbe96f8d03eaffb0381f3f0349b348`.
  No version string or revision is embedded in the binary; the JPL
  export-information file (`D:\ephem\jpl\README.txt`,
  "JPL PLANETARY AND LUNAR EPHEMERIDES : Export Information
  [21 December 2020]") documents DE441 (no lunar core/mantle damping,
  modern AU 149597870.700 km) and the FTP source layout
  (`ftp://ssd.jpl.nasa.gov/pub/eph/planets/...`, Linux binaries and
  ASCII/testpo siblings).
- Testpo extraction rule (verified byte-exact): keep the seven-token
  `testpo.441` data lines whose JD (token index 2) falls in
  [2451536.5, 2455216.5]; write them byte-identical. This yields
  exactly the 121 committed rows in order (first
  `441  2000.01.01 2451544.5 12  4  5 ...`, last
  `441  2010.01.01 2455197.5 15  0  2 ...`, all first-of-month). The
  242-line raw in-span set also contains 121 five-token fused lines
  (deep-past calendar labels such as `441-11434.01.01-2455210.5`);
  those are excluded by the seven-token rule. Verified with the
  repository's own fused-label parsing convention (last-dash JD tail).

Outstanding evidence gaps (BLOCKED, not verified):

- The original `sub_eph DE441.eph <out> 2000 2010` invocation (exact
  reviewed paths, shell history, operator) was not recovered from any
  log; the manifest's
  `tool="sub_eph DE441.eph <out> 2000 2010"` records the command shape
  from the commit message, not an observed shell transcript.
- No transcript proves the committed 952848-byte slice was produced by
  re-running that command against the 2788676624-byte source above;
  the slice's header coverage (2451536.5-2455216.5) and byte size are
  consistent with a 2000-2010 cut but that is consistency, not a
  witnessed reproduction. Re-running `sub_eph` now would write a new
  file, not prove the history of the committed bytes.
- `sub_eph.exe` carries no recoverable revision identifier
  (no `--version`, no embedded tag found); the tool identity above is
  a hash of the present binary, not a versioned release reference.
- Public download URLs/hashes for the full DE441 pair are not pinned
  here: the manifest's download block covers DE440/DE440t only, and
  the local `checksums.txt` DE441 span was not cross-checked against
  a JPL-published digest in this task.

Policy: if an authoritative reconstruction differs from the committed
bytes, obtain review before replacing data rather than changing the
expected hash automatically. Do not invent missing metadata or label
the gaps above verified.
