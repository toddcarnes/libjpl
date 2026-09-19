# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Disposable byte-exact fixture mutator for the T17 negative tests.
# Reads fixture paths from argv, writes only build-tree outputs, and never
# touches the committed source files. Invoked by
# tests/cmake/test_fixture_integrity.cmake via the system python3.
import sys


def read_lines(path):
    with open(path, "rb") as handle:
        return handle.read().replace(b"\r\n", b"\n").splitlines()


def identity_rows(text_lines):
    rows = []
    for line in text_lines:
        toks = line.split()
        rows.append(b" ".join(toks[2:6]))
    return rows


def main():
    mode = sys.argv[1]
    if mode == "flip":
        # Flip the high byte of an Earth-rotation (T12) coefficient double
        # at offset 18855 (record 1 starts at 2*8144 = 16288; the LE
        # double 32831107.1005468 at 18848 feeds row 1, T12 C4 X5 at JD
        # 2451544.5). Verified: flipping this byte makes the fixture
        # runner fail line 1 with MISMATCH, while keeping the file a
        # valid length and layout. (A low-byte flip at 18848 stays inside
        # the 5e-13 reference tolerance, so the high byte is used.)
        (_, out_path) = (sys.argv[2], sys.argv[2])
        with open(out_path, "r+b") as handle:
            handle.seek(18855)
            byte = handle.read(1)
            handle.seek(18855)
            handle.write(bytes([byte[0] ^ 0xFF]))
        return
    if mode == "dup":
        (_, out_text, _, out_rows) = sys.argv[2:6]
        lines = read_lines(sys.argv[2])
        assert len(lines) == 121
        dup = [lines[0]] + [lines[0]] * 119 + [lines[-1]]
        with open(out_text, "wb") as handle:
            handle.write(b"\n".join(dup) + b"\n")
        with open(out_rows, "wb") as handle:
            handle.write(b"\n".join(identity_rows(dup)) + b"\n")
        return
    if mode == "retarget":
        (_, out_text, _, out_rows) = sys.argv[2:6]
        lines = read_lines(sys.argv[2])
        assert len(lines) == 121
        # Row 61 (index 60) is "441  2005.01.01 2453371.5  1  9  6 ...":
        # change target 1 -> 2, keeping column width identical.
        assert lines[60].split()[3:5] == [b"1", b"9"]
        lines[60] = lines[60].replace(b"2453371.5  1  9  6", b"2453371.5  2  9  6")
        with open(out_text, "wb") as handle:
            handle.write(b"\n".join(lines) + b"\n")
        with open(out_rows, "wb") as handle:
            handle.write(b"\n".join(identity_rows(lines)) + b"\n")
        return
    if mode == "recoord":
        (_, out_text) = sys.argv[2:4]
        lines = read_lines(sys.argv[2])
        assert len(lines) == 121
        # Row 90 (index 89) is T1 C12 X5 with coordinate
        # -0.02293917668434295246: add 1e-9 (far above the 5e-13
        # reference tolerance) by flipping a significant digit while
        # keeping the line grammar valid. A last-decimal-digit flip
        # would vanish below binary64 resolution (delta 0.0), so the
        # 7th fractional digit 6 -> 7 is changed instead.
        assert b"-0.02293917668434295246" in lines[89]
        lines[89] = lines[89].replace(
            b"-0.02293917668434295246", b"-0.02293927668434295246")
        with open(out_text, "wb") as handle:
            handle.write(b"\n".join(lines) + b"\n")
        return
    if mode == "crlf":
        (_, out_text, _, out_rows) = sys.argv[2:6]
        with open(sys.argv[2], "rb") as handle:
            raw = handle.read()
        canonical = raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        with open(out_text, "wb") as handle:
            handle.write(canonical)
        with open(sys.argv[4], "rb") as handle:
            rows_raw = handle.read()
        rows_crlf = rows_raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        with open(out_rows, "wb") as handle:
            handle.write(rows_crlf)
        return
    raise SystemExit("unknown mutation mode: %r" % (mode,))


main()
