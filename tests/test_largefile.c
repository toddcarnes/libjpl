/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <io.h>
#define LF_FSEEK _fseeki64
#define LF_FTELL _ftelli64
typedef __int64 lf_offset;
#else
#define LF_FSEEK fseeko
#define LF_FTELL ftello
typedef off_t lf_offset;
#endif

enum { LF_RECORD_BYTES = 4096 };
enum { LF_RECORD_DOUBLES = LF_RECORD_BYTES / 8 };

/* 786432 advertised one-day data intervals: header + constants + data
 * records at 4096 bytes each give a 3221233664-byte file whose last-record
 * offset (3221229568) lies beyond the signed 32-bit range. */
#define LF_INTERVALS ((size_t)786432)
#define LF_START_JD 2451545.0
#define LF_STEP_DAYS 1.0
#define LF_END_JD (LF_START_JD + (double)LF_INTERVALS * LF_STEP_DAYS)
#define LF_MARK_SLOT ((size_t)510)
#define LF_MARK_VALUE 123456789.0

static const char *LF_PATH = "jpl_test_largefile.bin";

static uint64_t lf_last_offset(void) {
    return (uint64_t)(LF_INTERVALS + 1) * (uint64_t)LF_RECORD_BYTES;
}

static uint64_t lf_file_size(void) {
    return (uint64_t)(LF_INTERVALS + 2) * (uint64_t)LF_RECORD_BYTES;
}

static void lf_build_header(unsigned char *rec) {
    memset(rec, 0, LF_RECORD_BYTES);
    memcpy(rec, "largefile test", 14);
    test_mock_header(rec, LF_START_JD, LF_END_JD, LF_STEP_DAYS, 2);
    /* Sun-like span: start 2, 3*170*1 coefficients, end 511 of 512. */
    test_put_ptr(rec, 11, 2, 170, 1);
    test_put_ptr(rec, 12, 441, 0, 0);
}

static void lf_build_record(unsigned char *rec, double first, double second, double mark) {
    size_t i;
    memset(rec, 0, LF_RECORD_BYTES);
    test_put_f64(rec + 0, first);
    test_put_f64(rec + 8, second);
    for (i = 2; i < (size_t)LF_RECORD_DOUBLES; ++i)
        test_put_f64(rec + i * 8, (double)i);
    test_put_f64(rec + LF_MARK_SLOT * 8, mark);
}

/* Best-effort sparse flag so the multi-gigabyte hole does not allocate
 * zero clusters on filesystems (NTFS) that need an explicit mark. */
static void lf_make_sparse(FILE *f) {
#if defined(_WIN32)
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &written, NULL);
    }
#else
    (void)f;
#endif
}

static int lf_write_at(FILE *f, uint64_t offset, const unsigned char *rec) {
    if (LF_FSEEK(f, (lf_offset)offset, SEEK_SET) != 0) return 0;
    return fwrite(rec, 1, LF_RECORD_BYTES, f) == LF_RECORD_BYTES;
}

static int lf_check_size(FILE *f, uint64_t expected) {
    lf_offset end;
    if (LF_FSEEK(f, 0, SEEK_END) != 0) return 0;
    end = LF_FTELL(f);
    return end >= 0 && (uint64_t)end == expected;
}

int main(void) {
    unsigned char rec[LF_RECORD_BYTES];
    double vals[LF_RECORD_DOUBLES];
    FILE *f;
    jpl_ephemeris *e = NULL;

    JPL_TEST_CHECK(lf_last_offset() > (uint64_t)INT32_MAX);
    JPL_TEST_CHECK(lf_file_size() <= (uint64_t)SIZE_MAX);

    f = fopen(LF_PATH, "wb");
    JPL_TEST_CHECK(f != NULL);
    lf_make_sparse(f);
    lf_build_header(rec);
    JPL_TEST_CHECK(lf_write_at(f, 0, rec));
    lf_build_record(rec, 1.0, 2.0, 0.0);
    JPL_TEST_CHECK(lf_write_at(f, LF_RECORD_BYTES, rec));
    lf_build_record(rec, LF_START_JD, LF_START_JD + LF_STEP_DAYS, 111.0);
    JPL_TEST_CHECK(lf_write_at(f, (uint64_t)2 * LF_RECORD_BYTES, rec));
    lf_build_record(rec, LF_END_JD - LF_STEP_DAYS, LF_END_JD, LF_MARK_VALUE);
    JPL_TEST_CHECK(lf_write_at(f, lf_last_offset(), rec));
    if (!lf_check_size(f, lf_file_size())) {
        fclose(f);
        remove(LF_PATH);
        printf("SKIP: filesystem cannot create 3.2 GiB sparse fixture\n");
        return 0;
    }
    JPL_TEST_CHECK(fclose(f) == 0);

    /* No parser read may exceed 2880 bytes, even for this 4096-byte
     * record width. The hook reports over-limit requests as I/O failures,
     * so a passing open under the limit proves bounded header reads. */
    jpl_test_set_parser_read_limit(2880);
    {
        jpl_status lse = jpl_open(LF_PATH, &e);
        jpl_test_set_parser_read_limit(0);
        JPL_TEST_CHECK(lse == JPL_OK);
    }
    JPL_TEST_CHECK(e != NULL);
    JPL_TEST_CHECK(jpl_record_count(e) == LF_INTERVALS);
    JPL_TEST_CHECK(jpl_read_record(e, 0, vals, LF_RECORD_DOUBLES) == JPL_OK);
    JPL_TEST_CHECK(vals[0] == LF_START_JD);
    JPL_TEST_CHECK(vals[1] == LF_START_JD + LF_STEP_DAYS);
    JPL_TEST_CHECK(vals[LF_MARK_SLOT] == 111.0);
    JPL_TEST_CHECK(jpl_read_record(e, LF_INTERVALS - 1, vals, LF_RECORD_DOUBLES) == JPL_OK);
    JPL_TEST_CHECK(vals[0] == LF_END_JD - LF_STEP_DAYS);
    JPL_TEST_CHECK(vals[1] == LF_END_JD);
    JPL_TEST_CHECK(vals[LF_MARK_SLOT] == LF_MARK_VALUE);
    jpl_close(e);
    e = NULL;

    /* 64-bit only: the 3.2 GiB fixture exceeds the 32-bit address
     * space, and 32-bit builds are unsupported. */
    {
        jpl_ephemeris *m = NULL;
        JPL_TEST_CHECK(jpl_open_mmap(LF_PATH, &m) == JPL_OK);
        JPL_TEST_CHECK(m != NULL);
        JPL_TEST_CHECK(jpl_read_record(m, LF_INTERVALS - 1, vals, LF_RECORD_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(vals[1] == LF_END_JD);
        JPL_TEST_CHECK(vals[LF_MARK_SLOT] == LF_MARK_VALUE);
        jpl_close(m);
    }
    JPL_TEST_CHECK(remove(LF_PATH) == 0);

    puts("largefile tests passed");
    return 0;
}
