/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Directory sweep over named DE binaries (test-only).
 * Usage: test_all_ephemerides <directory>
 * ENOENT/ENOTDIR classify as absent (skip); permission/other stat errors
 * fail; any existing path whose open/read fails fails. */
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <sys/stat.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>
#endif

/* Smallest DE record width in bytes (classic header size, shared
 * JPL_TEST_RECORD_BYTES): a shorter named file cannot be a valid DE and
 * fails as present-but-corrupt. */

#define NAME_MAX_BYTES 260
#define MAX_NAMES 256

static int name_compare(const void *a, const void *b) {
#if defined(_WIN32)
    return _stricmp((const char *)a, (const char *)b);
#else
    return strcasecmp((const char *)a, (const char *)b);
#endif
}

static int is_eph_name(const char *name) {
    size_t n = strlen(name);
    if (n <= 4) return 0;
    return name_compare(name + n - 4, ".eph") == 0;
}

static int is_de404(const char *name) {
    return name_compare(name, "DE404.eph") == 0;
}

/* Collect every DE*.eph entry in the directory. DE404 is explicitly
 * excluded: docs/validation.md records no authoritative layout support
 * for it, so a local copy must not turn the sweep into a failure. */
static size_t collect_names(const char *dir, char names[][NAME_MAX_BYTES], size_t max) {
    size_t count = 0;
#if defined(_WIN32)
    char pattern[1024];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (snprintf(pattern, sizeof pattern, "%s\\*.eph", dir) < 0) return 0;
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) return 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            is_eph_name(data.cFileName) && !is_de404(data.cFileName) &&
            count < max) {
            if (snprintf(names[count], NAME_MAX_BYTES, "%s", data.cFileName) > 0)
                ++count;
        }
    } while (FindNextFileA(handle, &data) && count < max);
    FindClose(handle);
#else
    DIR *dirp = opendir(dir);
    struct dirent *entry;
    if (!dirp) return 0;
    while ((entry = readdir(dirp)) != NULL && count < max) {
        if (is_eph_name(entry->d_name) && !is_de404(entry->d_name)) {
            if (snprintf(names[count], NAME_MAX_BYTES, "%s", entry->d_name) > 0)
                ++count;
        }
    }
    closedir(dirp);
#endif
    qsort(names, count, NAME_MAX_BYTES, name_compare);
    return count;
}

int main(int argc, char **argv) {
    char names[MAX_NAMES][NAME_MAX_BYTES];
    char path[1024];
    size_t i;
    size_t name_count;
    size_t opened = 0;
    if (argc != 2) return 2;
    name_count = collect_names(argv[1], names, MAX_NAMES);
    for (i = 0; i < name_count; ++i) {
        jpl_ephemeris *ephemeris = NULL;
        const jpl_header *header;
        jpl_status status;
        double *record;
        FILE *probe;
        int n = snprintf(path, sizeof path, "%s/%s", argv[1], names[i]);
        if (n < 0 || (size_t)n >= sizeof path) return 2;
        /* Classify absence before opening: only ENOENT/ENOTDIR skip.
         * A privileged CI account reading a permission-restricted file
         * reports that platform case separately instead of passing the
         * permission branch. */
        probe = fopen(path, "rb");
        if (probe == NULL) {
            if (errno == ENOENT
#if defined(ENOTDIR) && ENOTDIR != ENOENT
                || errno == ENOTDIR
#endif
            ) {
                continue;
            }
#if defined(_WIN32)
            /* MSVC maps Windows errors into errno; ERROR_ACCESS_DENIED
             * arrives as EACCES. Report readability explicitly. */
            if (errno == EACCES) {
#else
            /* POSIX: a privileged account may still read a mode-000 file;
             * report that case separately instead of passing silently. */
            if (errno == EACCES || errno == EPERM) {
#endif
                FILE *acc = NULL;
#if defined(_MSC_VER)
                if (fopen_s(&acc, path, "rb") == 0 && acc != NULL) {
                    fclose(acc);
                    printf("%s: readable by this account; permission branch not exercised here\n",
                           names[i]);
                    continue;
                }
#else
                acc = fopen(path, "rb");
                if (acc != NULL) {
                    fclose(acc);
                    printf("%s: readable by this account; permission branch not exercised here\n",
                           names[i]);
                    continue;
                }
#endif
            }
            fprintf(stderr, "%s: cannot stat/open present file: %s\n",
                    names[i], strerror(errno));
            return 1;
        }
        fclose(probe);
        /* The path exists (probe opened it): every later failure is a
         * present-file failure. A directory or other non-regular path
         * must fail rather than skip; a corrupt named DE file must fail
         * at open or decode, never read as absence. */
        {
            /* 64-bit size probe: long/ftell overflow on multi-gigabyte
             * files (e.g. DE431/DE441), so use _stat64 on Windows and
             * stat/fstat with large-file offsets on POSIX. */
#if defined(_WIN32)
            struct __stat64 st;
            if (_stat64(path, &st) != 0) {
                fprintf(stderr, "%s: cannot stat present file: %s\n",
                        names[i], strerror(errno));
                return 1;
            }
            if (st.st_size < 0 ||
                (uintmax_t)st.st_size < (uintmax_t)JPL_TEST_RECORD_BYTES) {
                fprintf(stderr, "%s: present file too short (%lld bytes)\n",
                        names[i], (long long)st.st_size);
                return 1;
            }
#else
            struct stat st;
            if (stat(path, &st) != 0) {
                fprintf(stderr, "%s: cannot stat present file: %s\n",
                        names[i], strerror(errno));
                return 1;
            }
            if (st.st_size < 0 ||
                (uintmax_t)st.st_size < (uintmax_t)JPL_TEST_RECORD_BYTES) {
                fprintf(stderr, "%s: present file too short (%lld bytes)\n",
                        names[i], (long long)st.st_size);
                return 1;
            }
#endif
        }
        status = jpl_open(path, &ephemeris);
        if (status != JPL_OK) {
            /* Present-but-unreadable files fail. Truncated/corrupt named
             * files report FORMAT through the shared validator; shorter
             * short reads report IO. Either is a present-file failure. */
            fprintf(stderr, "%s: present file failed to open: %s\n",
                    names[i], jpl_status_string(status));
            return 1;
        }
        header = jpl_get_header(ephemeris);
        if (header == NULL || header->record_double_count < 3 || jpl_record_count(ephemeris) == 0) {
            fprintf(stderr, "%s: invalid header\n", names[i]);
            jpl_close(ephemeris);
            return 1;
        }
        record = malloc(header->record_double_count * sizeof *record);
        if (record == NULL) {
            fprintf(stderr, "%s: out of memory\n", names[i]);
            jpl_close(ephemeris);
            return 1;
        }
        /* Decode and validate every physical record, not just record
         * zero. Each record needs finite ordered encoded dates; covered
         * records anchor at coverage start/end with continuity, and one
         * trailing record may start at coverage end staying ordered. */
        {
            size_t count = jpl_record_count(ephemeris);
            size_t index;
            double prev_end[2] = {0.0, 0.0};
            double last_start[2] = {0.0, 0.0};
            int have_prev = 0;
            double cov_start[2], cov_end[2];
            if (jpl_date_normalize(header->start_jd, cov_start) != JPL_OK ||
                jpl_date_normalize(header->end_jd, cov_end) != JPL_OK) {
                fprintf(stderr, "%s: record %zu: nonfinite coverage\n", names[i], (size_t)0);
                free(record);
                jpl_close(ephemeris);
                return 1;
            }
            for (index = 0; index < count; ++index) {
                double rec_start[2], rec_end[2];
                status = jpl_read_record(ephemeris, index, record, header->record_double_count);
                if (status != JPL_OK) {
                    fprintf(stderr, "%s: record %zu: could not decode: %s\n",
                            names[i], index, jpl_status_string(status));
                    free(record);
                    jpl_close(ephemeris);
                    return 1;
                }
                rec_start[0] = record[0]; rec_start[1] = 0.0;
                rec_end[0] = record[1]; rec_end[1] = 0.0;
                if (jpl_date_normalize(rec_start, rec_start) != JPL_OK ||
                    jpl_date_normalize(rec_end, rec_end) != JPL_OK ||
                    !isfinite(rec_start[0]) || !isfinite(rec_start[1]) ||
                    !isfinite(rec_end[0]) || !isfinite(rec_end[1])) {
                    fprintf(stderr, "%s: record %zu: nonfinite record dates\n", names[i], index);
                    free(record);
                    jpl_close(ephemeris);
                    return 1;
                }
                if (!(jpl_date_compare(rec_end, rec_start) > 0)) {
                    fprintf(stderr, "%s: record %zu: unordered record dates\n", names[i], index);
                    free(record);
                    jpl_close(ephemeris);
                    return 1;
                }
                if (have_prev && jpl_date_compare(rec_start, prev_end) != 0) {
                    fprintf(stderr, "%s: record %zu: discontinuous record dates\n", names[i], index);
                    free(record);
                    jpl_close(ephemeris);
                    return 1;
                }
                if (index == 0 && jpl_date_compare(rec_start, cov_start) != 0) {
                    fprintf(stderr, "%s: record %zu: not anchored at coverage start\n", names[i], index);
                    free(record);
                    jpl_close(ephemeris);
                    return 1;
                }
                prev_end[0] = rec_end[0]; prev_end[1] = rec_end[1];
                last_start[0] = rec_start[0]; last_start[1] = rec_start[1];
                have_prev = 1;
            }
            /* Final covered record ends at coverage end; alternatively one
             * trailing record starts exactly at coverage end and stays
             * ordered (continuity and ordering already checked above).
             * Anything else -- ending early or extending past one trailing
             * record -- fails here. */
            if (jpl_date_compare(prev_end, cov_end) != 0 &&
                jpl_date_compare(last_start, cov_end) != 0) {
                fprintf(stderr, "%s: record %zu: not anchored at coverage end\n",
                        names[i], count - 1);
                free(record);
                jpl_close(ephemeris);
                return 1;
            }
        }
        free(record);
        jpl_close(ephemeris);
        opened++;
    }
    if (opened == 0) {
        fputs("no JPL DE binaries found in directory\n", stderr);
        return 1;
    }
    printf("all present JPL DE binaries decoded: every physical record checked (skipped absent files)\n");
    return 0;
}
