/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif
#include "jpl.h"
#include "jpl_private.h"

#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define JPL_FSEEK _fseeki64
#define JPL_FTELL _ftelli64
typedef __int64 jpl_file_offset;
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define JPL_FSEEK fseeko
#define JPL_FTELL ftello
typedef off_t jpl_file_offset;
_Static_assert(sizeof(jpl_file_offset) == sizeof(int64_t), "64-bit file offsets required");
#endif

/* Classic header window also floors the minimum physical record width;
 * MIN_RECORD_BYTES aliases it (see geometry bound below). */
enum { CLASSIC_HEADER_BYTES = 2856 };
enum { MIN_RECORD_BYTES = CLASSIC_HEADER_BYTES };
_Static_assert(MIN_RECORD_BYTES == 2856, "classic header/minimum record widths agree");
_Static_assert((size_t)JPL_TT_TDB_TAIL_BASE + 24 > (size_t)CLASSIC_HEADER_BYTES,
    "classic pre-parse always truncates the TT-TDB tail");

/* Byte codecs, offset helpers, and the header parser are defined below
 * the candidate validator (which calls them), so forward-declare them
 * here to keep one definition each (sole forward-declaration block). */
static uint32_t read_u32(const unsigned char *p, jpl_byte_order order);
static double read_f64(const unsigned char *p, jpl_byte_order order);
static int seek_to(FILE *file, size_t offset);
static int checked_record_offset(size_t index, size_t record_doubles, size_t *out);
static jpl_status parse_header(jpl_header *header, const unsigned char *bytes, size_t size,
                                jpl_byte_order order);
static jpl_reference_frame classify_frame(int denum);

struct jpl_ephemeris {
    FILE *file;
    jpl_header header;
    size_t file_size;
    size_t record_count;
    /* File identity captured from the open stream (not a path lookup),
     * used to verify FILE clones reopened the same object. POSIX stores
     * st_dev/st_ino; Windows stores volume serial + file index from the
     * underlying handle. file_size mirrors st_size/GetFileSizeEx and is
     * re-checked in the clone comparison. Contents must stay immutable
     * while any reader is active: identity validation is not snapshot
     * isolation against in-place edits. */
#if !defined(_WIN32)
    dev_t id_dev;
    ino_t id_ino;
#else
    DWORD id_volume;
    DWORD id_index_hi;
    DWORD id_index_lo;
#endif
    /* Shared ownership counter: mmap clones share one heap counter; plain
     * FILE handles own a private count of 1. Windows uses Interlocked* on
     * an int (MSVC lacks C11 atomics without /experimental:c11atomics);
     * POSIX uses C17 atomic_int. Both make concurrent clone+close safe
     * (clone-vs-close of the last sibling still needs external sync).
     * See jpl_ephemeris_clone. */
#if defined(_WIN32)
    int *refcount;
#else
    atomic_int *refcount;
#endif
    int is_mmap;
    unsigned char *mmap_base;
    size_t mmap_size;
    /* Single-record cache: last record_index + its decoded doubles. Used by
     * FILE-path reads only (mmap reads bypass it to stay lock-free); valid
     * only when cache_valid != 0. Clones always start cold (calloc) so no
     * cross-handle state is shared. */
    double *cache_record;
    size_t cache_index;
    int cache_valid;
    /* Copy of path for plain FILE handles so clones can fopen independently.
     * NULL for mmap handles (clones share the mapping instead). */
    char *path;
#if defined(_WIN32)
    HANDLE hFile;
    HANDLE hMap;
#endif
};

/* Capture stream identity from an open FILE: POSIX fstat device+inode,
 * Windows volume/file index from the OS handle behind the CRT stream.
 * Returns 0 on success, -1 when identity cannot be determined. The size
 * comparison lives in stream_identity_matches against the live source
 * stream and expected file_size; capture only records the id fields. */
static int capture_stream_identity(FILE *file, struct jpl_ephemeris *e) {
#if !defined(_WIN32)
    struct stat st;
    if (!file || !e) return -1;
    if (fstat(fileno(file), &st) != 0) return -1;
    e->id_dev = st.st_dev;
    e->id_ino = st.st_ino;
    return 0;
#else
    HANDLE h;
    BY_HANDLE_FILE_INFORMATION info;
    if (!file || !e) return -1;
    h = (HANDLE)_get_osfhandle(_fileno(file));
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!GetFileInformationByHandle(h, &info)) return -1;
    e->id_volume = info.dwVolumeSerialNumber;
    e->id_index_hi = info.nFileIndexHigh;
    e->id_index_lo = info.nFileIndexLow;
    return 0;
#endif
}

/* Compare a reopened candidate stream against the source handle's live
 * identity plus expected size. Compares device+inode (POSIX) or
 * volume/file index (Windows) from the two open streams, plus the
 * reopened size against the source stream's live size and the expected
 * file_size metadata. Returns 0 when all match, -1 otherwise. */
static int stream_identity_matches(const struct jpl_ephemeris *source, FILE *candidate) {
#if !defined(_WIN32)
    struct stat original, reopened;
    if (!source || !source->file || !candidate) return -1;
    if (fstat(fileno(source->file), &original) != 0) return -1;
    if (fstat(fileno(candidate), &reopened) != 0) return -1;
    if (original.st_dev != reopened.st_dev || original.st_ino != reopened.st_ino) return -1;
    if (original.st_size != reopened.st_size) return -1;
    if ((uintmax_t)reopened.st_size != (uintmax_t)source->file_size) return -1;
    return 0;
#else
    BY_HANDLE_FILE_INFORMATION oinfo, rinfo;
    LARGE_INTEGER osize, rsize;
    HANDLE ho, hr;
    if (!source || !source->file || !candidate) return -1;
    ho = (HANDLE)_get_osfhandle(_fileno(source->file));
    hr = (HANDLE)_get_osfhandle(_fileno(candidate));
    if (ho == INVALID_HANDLE_VALUE || hr == INVALID_HANDLE_VALUE) return -1;
    if (!GetFileInformationByHandle(ho, &oinfo)) return -1;
    if (!GetFileInformationByHandle(hr, &rinfo)) return -1;
    if (oinfo.dwVolumeSerialNumber != rinfo.dwVolumeSerialNumber ||
        oinfo.nFileIndexHigh != rinfo.nFileIndexHigh ||
        oinfo.nFileIndexLow != rinfo.nFileIndexLow) return -1;
    if (!GetFileSizeEx(ho, &osize) || !GetFileSizeEx(hr, &rsize)) return -1;
    if (osize.QuadPart != rsize.QuadPart) return -1;
    if ((uintmax_t)rsize.QuadPart != (uintmax_t)source->file_size) return -1;
    return 0;
#endif
}

static char *copy_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* Resolve a relative reopen path to an absolute path before fopen so a
 * later cwd change cannot redirect the clone to a different file. Uses
 * _fullpath on Windows and realpath(NULL) on POSIX (malloc'd; caller
 * frees). Falls back to a plain copy when resolution fails (clone then
 * re-verifies identity and fails IO rather than reading wrong data). */
static char *resolve_reopen_path(const char *path) {
    if (!path) return NULL;
#if defined(_WIN32)
    {
        char *resolved = (char *)malloc(_MAX_PATH);
        if (!resolved) return NULL;
        if (!_fullpath(resolved, path, _MAX_PATH)) { free(resolved); return copy_string(path); }
        return resolved;
    }
#else
    {
        char *resolved = realpath(path, NULL);
        if (!resolved) return copy_string(path);
        return resolved;
    }
#endif
}

#if defined(_WIN32)
/* Interlocked* operates on LONG; int and LONG are both 32-bit on Windows. */
_Static_assert(sizeof(int) == sizeof(LONG), "libjpl refcount must match LONG");
static void refcount_init(int *r) { *r = 1; }
static void refcount_inc(int *r) { InterlockedIncrement((volatile LONG *)r); }
/* Returns the remaining count after decrement. */
static int refcount_dec(int *r) { return InterlockedDecrement((volatile LONG *)r); }
#else
static void refcount_init(atomic_int *r) { atomic_init(r, 1); }
static void refcount_inc(atomic_int *r) { atomic_fetch_add(r, 1); }
/* Returns the remaining count after decrement (fetch_sub yields previous). */
static int refcount_dec(atomic_int *r) { return atomic_fetch_sub(r, 1) - 1; }
#endif

/* TT-TDB span end (1-based slot count) for a parsed pointer: computes
 * (start - 1) + coeff * sub with overflow guards. Returns 0 on success,
 * -1 when the triple is absent (non-positive) or overflows. Guards in
 * order: start/coeff/sub are strictly positive so (size_t)start - 1
 * cannot underflow; the product guard rejects coeff*sub overflow before
 * multiplying; the sum guard rejects (start-1)+product overflow before
 * adding. Callers treat any -1 as absent without enlarging the record. */
static int tt_tdb_span_end(const jpl_pointer *pointer, size_t *out) {
    size_t product, end;
    if (!pointer || !out) return -1;
    if (pointer->start_index <= 0 || pointer->coefficients_per_component <= 0 ||
        pointer->subintervals <= 0) return -1;
    if ((size_t)pointer->subintervals > SIZE_MAX / (size_t)pointer->coefficients_per_component) return -1;
    product = (size_t)pointer->coefficients_per_component * (size_t)pointer->subintervals;
    if (product > SIZE_MAX - ((size_t)pointer->start_index - 1)) return -1;
    end = ((size_t)pointer->start_index - 1) + product;
    *out = end;
    return 0;
}

/* Minimum physical record width bound (see CLASSIC_HEADER_BYTES above):
 * The interval count is bounded by file geometry (file_size divided by
 * this width, minus header/constants records) before any
 * float-to-integer conversion. SIZE_MAX / 2856 stays below 2^53 on every
 * supported size_t width (about 6.5e15 on 64-bit, 1.5e6 on 32-bit), so
 * the bound is exactly representable as a double and no SIZE_MAX-to-double
 * cast is ever used as a guard. */
_Static_assert((uintmax_t)(SIZE_MAX / MIN_RECORD_BYTES) < 9007199254740992ULL,
    "geometry bound must stay below the exact-double integer limit");

/* Byte-source callbacks follow; the sole forward-declaration block above
 * already declares the codecs/seek/parse_header helpers used below. */

/* Bounded storage callbacks: FILE and mmap submit raw bytes through
 * the same jpl_byte_source interface so both open paths run one shared
 * validation policy. Every read checks `offset <= size` and
 * `count <= size - offset` with subtraction guarded by the first check;
 * mmap memcpy and FILE seek/read share error classification (IO vs
 * FORMAT stays with the candidate evaluator). */
typedef jpl_status (*jpl_read_bytes_fn)(void *context, size_t offset,
                                        unsigned char *out, size_t count);
typedef struct jpl_byte_source {
    void *context;
    size_t size;
    jpl_read_bytes_fn read;
} jpl_byte_source;

typedef struct jpl_mmap_source { const unsigned char *base; size_t size; } jpl_mmap_source;
typedef struct jpl_file_source { FILE *file; size_t size; } jpl_file_source;

static jpl_status mmap_read_bytes(void *context, size_t offset,
                                  unsigned char *out, size_t count) {
    const jpl_mmap_source *source = (const jpl_mmap_source *)context;
    if (!source || !source->base || !out) return JPL_ERROR_ARGUMENT;
    if (offset > source->size || count > source->size - offset) return JPL_ERROR_IO;
    memcpy(out, source->base + offset, count);
    return JPL_OK;
}

static jpl_status file_read_bytes(void *context, size_t offset,
                                  unsigned char *out, size_t count) {
    const jpl_file_source *source = (const jpl_file_source *)context;
    if (!source || !source->file || !out) return JPL_ERROR_ARGUMENT;
    if (offset > source->size || count > source->size - offset) return JPL_ERROR_IO;
    if (seek_to(source->file, offset) != 0) return JPL_ERROR_IO;
    return fread(out, 1, count, source->file) == count ? JPL_OK : JPL_ERROR_IO;
}

/* Scheduled data-record endpoint as a normalized two-part date: the
 * advertised coverage start pair plus index*step, computed with the private
 * compensated helpers. The decoded scalar header epochs are promoted to
 * {encoded_value, 0.0} only here at the decoding boundary; every query
 * comparison below carries both parts. */
static jpl_status scheduled_endpoint(const double start[2], double step, size_t index,
                                     double out[2]) {
    double coverage[2];
    jpl_status status;
    if (!start || !out) return JPL_ERROR_FORMAT;
    if (!isfinite(start[0]) || !isfinite(start[1]) || !isfinite(step) || step <= 0.0)
        return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(start, coverage);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_add_scaled(coverage, step, index, out);
    return status == JPL_OK ? JPL_OK : JPL_ERROR_FORMAT;
}

/* Reconstructed endpoints are compared to encoded file endpoints with a
 * documented bound derived from their binary64 encoding ULPs, not from a
 * coverage tolerance. One encoded double near 2.4e6 carries an ULP of
 * 2^-30 (~9.3e-10 day); the scheduled endpoint adds at most one rounding
 * of similar size, so accept |scheduled - encoded| <= 4 * 2^-30 days
 * (~3.7e-9 day, ~0.3 microsecond). This allowance excuses only rounding
 * already present in the encoded file dates; public coverage queries in
 * interpolate()/jpl_get_state() use exact comparisons, never this bound. */
#define JPL_T05_ENDPOINT_TOL_DAYS (4.0 * 9.313225746154785e-10)

static jpl_status endpoints_match(const double scheduled[2], double encoded) {
    double error;
    if (!isfinite(scheduled[0]) || !isfinite(scheduled[1]) || !isfinite(encoded)) return JPL_ERROR_FORMAT;
    error = fabs((scheduled[0] - encoded) + scheduled[1]);
    if (!isfinite(error) || error > JPL_T05_ENDPOINT_TOL_DAYS) return JPL_ERROR_FORMAT;
    return JPL_OK;
}

static jpl_status header_record_endpoints(const jpl_byte_source *source, size_t offset,
                                          size_t record_bytes, jpl_byte_order order,
                                          double first[2], double second[2]) {
    unsigned char raw[16];
    jpl_status status;
    double enc0, enc1;
    if (!source || !first || !second) return JPL_ERROR_ARGUMENT;
    if (record_bytes % 8 != 0 || record_bytes < (size_t)MIN_RECORD_BYTES) return JPL_ERROR_FORMAT;
    status = source->read(source->context, offset, raw, sizeof raw);
    if (status != JPL_OK) return status;
    enc0 = read_f64(raw, order);
    enc1 = read_f64(raw + 8, order);
    if (!isfinite(enc0) || !isfinite(enc1) || enc1 <= enc0) return JPL_ERROR_FORMAT;
    first[0] = enc0;
    first[1] = 0.0;
    second[0] = enc1;
    second[1] = 0.0;
    return JPL_OK;
}

/* Complete candidate validation: metadata, count, record width,
 * first/last/trailing endpoints, and checked classic spans. Parses into
 * the candidate-local header, never the published header. Returns JPL_OK
 * with *out_record_count set, or JPL_ERROR_FORMAT for an invalid
 * candidate, propagating actual I/O or allocation failures distinctly. */
static jpl_status validate_layout(const jpl_byte_source *source,
                                  jpl_byte_order order,
                                  jpl_header *header, size_t *record_count) {
    double span_days, span_records, reconstructed;
    size_t max_records, expected_records, total_records, record_bytes, extra_records;
    size_t required_doubles, required_bytes, width;
    unsigned char classic[CLASSIC_HEADER_BYTES];
    jpl_header candidate;
    jpl_status status;
    if (!source || !header || !record_count || !source->read) return JPL_ERROR_ARGUMENT;
    /* Initial header reads use the classic required size, not an
     * unconditional extension-sized read. Parse into the candidate-local
     * header, never the published header. Overflow-name headers (T06:
     * constant_count > 400) carry their IPT 14/15 tail past the classic
     * cursor, so extend the initial window past 2856 when the classic
     * pre-parse advertises overflow names -- bounded by the file size
     * so a corrupt count cannot over-allocate. Re-parse the extended
     * window under the same byte order before geometry checks. */
    status = source->read(source->context, 0, classic, sizeof classic);
    if (status != JPL_OK) return status;
    memset(&candidate, 0, sizeof candidate);
    status = parse_header(&candidate, classic, sizeof classic, order);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (candidate.constant_count > JPL_MAX_CONSTANT_NAMES) {
        size_t extra = (size_t)(candidate.constant_count - JPL_MAX_CONSTANT_NAMES);
        size_t need = 0;
        unsigned char *wide = NULL;
        jpl_header reparsed;
        if (extra > (SIZE_MAX - (size_t)JPL_TT_TDB_TAIL_BASE - 24) / (size_t)JPL_CONSTANT_NAME_LENGTH)
            return JPL_ERROR_FORMAT;
        need = (size_t)JPL_TT_TDB_TAIL_BASE + extra * (size_t)JPL_CONSTANT_NAME_LENGTH + 24;
        if (need <= sizeof classic || need > source->size) return JPL_ERROR_FORMAT;
        wide = malloc(need);
        if (!wide) return JPL_ERROR_MEMORY;
        status = source->read(source->context, 0, wide, need);
        if (status != JPL_OK) { free(wide); return status; }
        status = parse_header(&reparsed, wide, need, order);
        free(wide);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        candidate = reparsed;
    }
    if (source->size < (size_t)MIN_RECORD_BYTES) return JPL_ERROR_FORMAT;
    max_records = source->size / (size_t)MIN_RECORD_BYTES;
    if (max_records < 2) return JPL_ERROR_FORMAT;
    max_records -= 2; /* Header plus constants records; guarded above. */
    if (!isfinite(candidate.start_jd[0]) || !isfinite(candidate.start_jd[1]) ||
        !isfinite(candidate.end_jd[0]) || !isfinite(candidate.end_jd[1])) return JPL_ERROR_FORMAT;
    { double cov_start[2], cov_end[2], cov_span[2];
      double span_lo;
      jpl_status cov_status = jpl_date_normalize(candidate.start_jd, cov_start);
      if (cov_status != JPL_OK) return JPL_ERROR_FORMAT;
      cov_status = jpl_date_normalize(candidate.end_jd, cov_end);
      if (cov_status != JPL_OK) return JPL_ERROR_FORMAT;
      if (jpl_date_compare(cov_end, cov_start) <= 0) return JPL_ERROR_FORMAT;
      cov_status = jpl_date_subtract(cov_end, cov_start, cov_span);
      if (cov_status != JPL_OK) return JPL_ERROR_FORMAT;
      span_days = cov_span[0];
      span_lo = cov_span[1];
      if (!isfinite(candidate.step_days) || candidate.step_days <= 0.0) return JPL_ERROR_FORMAT;
      /* Encoded coverage endpoints are whole/half-day values; their span is
       * exactly representable, so combining the normalized parts is safe
       * here for the interval-count estimate only. All query indexing
       * below keeps both parts. */
      span_days += span_lo; }
    if (!isfinite(span_days) || span_days <= 0.0) return JPL_ERROR_FORMAT;
    span_records = span_days / candidate.step_days;
    if (!isfinite(span_records) || span_records < 1.0 || span_records > (double)max_records) return JPL_ERROR_FORMAT;
    expected_records = (size_t)(span_records + 0.5);
    if (expected_records == 0 || expected_records > max_records) return JPL_ERROR_FORMAT;
    if (fabs(span_records - (double)expected_records) > 1e-7) return JPL_ERROR_FORMAT;
    reconstructed = (double)expected_records * candidate.step_days;
    if (!isfinite(reconstructed) ||
        fabs(reconstructed - span_days) > 1e-7 * candidate.step_days) return JPL_ERROR_FORMAT;
    /* Required classic width: pointer extent and constants count floor
     * the DOUBLE requirement; the 2856-byte classic base floors the BYTE
     * requirement. Overflow-name bytes live inside the header record but
     * do NOT inflate the per-record width: every record (header,
     * constants, data) has the same width, and the overflow names
     * occupy header-record bytes that would otherwise be padding.
     * (Overflow names need no extra width: e.g. 1470 name bytes must not
     * be added to an 8144-byte pointer-derived width.) The full-record
     * re-parse below still locates the tail at 2856+6*extra inside
     * the width. */
    required_doubles = candidate.record_double_count;
    if (required_doubles == 0) return JPL_ERROR_FORMAT;
    if ((size_t)candidate.constant_count > required_doubles) required_doubles = (size_t)candidate.constant_count;
    if (required_doubles > SIZE_MAX / 8) return JPL_ERROR_FORMAT;
    required_bytes = required_doubles * 8;
    if (required_bytes < (size_t)MIN_RECORD_BYTES) required_bytes = (size_t)MIN_RECORD_BYTES;
    record_bytes = 0;
    total_records = 0;
    /* Width search order matters for the R06b trailing-record case:
     * each divisor width is checked with its own extra count (3 for
     * the trailing split, 2 for the plain split), trailing split
     * first. A width is accepted only when its candidate offsets carry
     * the scheduled first/last (and trailing, when present) endpoints
     * AND the full header record re-parses with matching optional
     * metadata. Prefer the trailing split when exactly one split
     * validates; fail when both validate with different widths. */
    {
        /* Each split is an independent width hypothesis: extra=3 first
         * (trailing-record files), then extra=2 (plain files). At most
         * one split may validate; both validating means the file
         * admits two physical readings (different totals, hence
         * different record counts) and is materially ambiguous (R06b).
         * A plain valid file validates exactly the extra=2 split; a
         * trailing-record file exactly the extra=3 split. */
        static const size_t extras[2] = {3, 2};
        size_t split_bytes[2] = {0, 0};
        size_t split_totals[2] = {0, 0};
        jpl_header split_headers[2];
        int split_ok[2] = {0, 0};
        size_t ei;
        for (ei = 0; ei < 2; ++ei) {
            double first[2] = {0.0, 0.0}, second[2] = {0.0, 0.0};
            double sched_first[2] = {0.0, 0.0}, sched_last[2] = {0.0, 0.0}, sched_trail[2] = {0.0, 0.0};
            double tfirst[2] = {0.0, 0.0}, tsecond[2] = {0.0, 0.0};
            size_t attempt_total = 0, attempt_bytes = 0;
            size_t off_first, off_last, off_trail;
            jpl_status ep_status;
            extra_records = extras[ei];
            if (expected_records > SIZE_MAX - extra_records) break;
            attempt_total = expected_records + extra_records;
            if (source->size % attempt_total != 0) continue;
            width = source->size / attempt_total;
            if (width % 8 != 0 || width < (size_t)MIN_RECORD_BYTES) continue;
            if (width < required_bytes) continue;
            if (width / 8 < required_doubles) continue;
            if (candidate.constant_count > 0 &&
                (size_t)candidate.constant_count > width / 8) continue;
            /* First covered record, last covered record, and optional
             * trailing record at their candidate offsets: header (0)
             * then constants (1) then data, so the first data record
             * is at 2*width. The constants record carries
             * constant_count doubles; data records carry scheduled
             * Julian dates at slots 0/1. */
            off_first = width;
            off_last = width + expected_records * width;
            off_trail = width + (expected_records + 1) * width;
            {
                size_t data_off = width + width;
                unsigned char raw0[8];
                double slot0;
                if (data_off / width != 2) continue;
                if (off_last / width != expected_records + 1) continue;
                if (extra_records == 3 && off_trail / width != expected_records + 2) continue;
                if (source->read(source->context, data_off, raw0, sizeof raw0) != JPL_OK) continue;
                slot0 = read_f64(raw0, order);
                if (!isnan(slot0)) {
                    /* Constants-identity discriminator (R06b): a phantom
                     * divisor reads the constants record as data. Real
                     * constants records hold constant_count values, so
                     * compare only when the candidate constants record
                     * actually HAS payload at slot 0 (constant_count >
                     * 0); a zero-payload constants record cannot
                     * discriminate. */
                    if (candidate.constant_count > 0) {
                        unsigned char cbuf[8];
                        double cslot0;
                        if (source->read(source->context, off_first, cbuf, sizeof cbuf) != JPL_OK) continue;
                        cslot0 = read_f64(cbuf, order);
                        if (cslot0 == slot0) continue;
                    }
                } else {
                    /* Corrupt first data date with no clean covered
                     * endpoint to anchor the layout: reject the width
                     * (single-record NaN-first files fail here; the
                     * lazy FORMAT path is covered by interior-corrupt
                     * files with clean endpoints). */
                    continue;
                }
                ep_status = header_record_endpoints(source, data_off, width, order,
                                                    first, second);
                if (ep_status != JPL_OK) continue;
            }
            /* Interior-corrupt files (T04 three-record case) carry a NaN
             * MIDDLE record with clean first/last records: they validate
             * normally below. */
            if (scheduled_endpoint(candidate.start_jd, candidate.step_days, 0, sched_first) != JPL_OK) continue;
            if (scheduled_endpoint(candidate.start_jd, candidate.step_days, 1, sched_last) != JPL_OK) continue;
            if (endpoints_match(sched_first, first[0]) != JPL_OK) continue;
            if (endpoints_match(sched_last, second[0]) != JPL_OK) continue;
            if (header_record_endpoints(source, off_last, width, order,
                                        first, second) != JPL_OK) continue;
            if (scheduled_endpoint(candidate.start_jd, candidate.step_days,
                                   expected_records - 1, sched_first) != JPL_OK) continue;
            if (scheduled_endpoint(candidate.start_jd, candidate.step_days,
                                   expected_records, sched_last) != JPL_OK) continue;
            if (endpoints_match(sched_first, first[0]) != JPL_OK) continue;
            if (endpoints_match(sched_last, second[0]) != JPL_OK) continue;
            if (extra_records == 3) {
                if (header_record_endpoints(source, off_trail, width, order,
                                            tfirst, tsecond) != JPL_OK) continue;
                if (scheduled_endpoint(candidate.start_jd, candidate.step_days,
                                       expected_records, sched_trail) != JPL_OK) continue;
                if (endpoints_match(sched_trail, tfirst[0]) != JPL_OK) continue;
                /* Trailing record is physical only; its end need not tie
                 * to advertised coverage (checked as ordered/finite). */
            }
            /* Full candidate header record must re-parse cleanly so
             * extension words come only from inside it, never from the
             * constants or data record. The optional TT-TDB triple is
             * tentative until geometry is known: after the candidate
             * width is established, locate 2856+6*extra with checked
             * arithmetic, require 24 tail bytes inside that header
             * record, and accept only a fully positive, checked,
             * in-record span. Anything else (missing tail, absent or
             * unrepresentable triple, overflow, span outside the
             * physical record) means absent without enlarging the
             * width or rejecting the classic file. */
            {
                unsigned char *full = malloc(width);
                jpl_header reparsed;
                size_t end = 0, tail = 0, tail_extra = 0;
                int tail_inside = 0;
                jpl_pointer tentative;
                jpl_status rstatus;
                if (!full) return JPL_ERROR_MEMORY;
                rstatus = source->read(source->context, 0, full, width);
                if (rstatus != JPL_OK) { free(full); return rstatus; }
                rstatus = parse_header(&reparsed, full, width, order);
                free(full);
                if (rstatus != JPL_OK) continue;
                if (reparsed.record_double_count > width / 8) continue;
                reparsed.record_double_count = width / 8;
                reparsed.has_tt_tdb = 0;
                /* Tail location inside the candidate header record. The byte
                 * tail is 2856 + 6*extra (extra = max(nconst-400,0)); the
                 * guard below rejects 6*extra overflow before multiplying,
                 * then rejects tail+24 overflow before adding. */
                tail_extra = reparsed.constant_count > JPL_MAX_CONSTANT_NAMES ?
                    (size_t)(reparsed.constant_count - JPL_MAX_CONSTANT_NAMES) : 0;
                if (tail_extra <= (SIZE_MAX - (size_t)JPL_TT_TDB_TAIL_BASE - 24) /
                        (size_t)JPL_CONSTANT_NAME_LENGTH) {
                    tail = (size_t)JPL_TT_TDB_TAIL_BASE +
                        tail_extra * (size_t)JPL_CONSTANT_NAME_LENGTH;
                    tail_inside = tail + 24 <= width ? 1 : 0;
                }
                tentative = reparsed.tt_tdb_pointer;
                memset(&reparsed.tt_tdb_pointer, 0, sizeof reparsed.tt_tdb_pointer);
                /* Span end is the 1-based count (start-1)+coeff*sub; end <=
                 * width accepts the boundary equality (end == width). Slot
                 * width itself holds no coefficient, so an accepted triple
                 * whose span ends exactly at the width still evaluates as
                 * absent downstream (no coefficient bytes to interpolate);
                 * only a strictly interior span yields usable TT-TDB. */
                if (tail_inside &&
                    tt_tdb_span_end(&tentative, &end) == 0 &&
                    end <= reparsed.record_double_count) {
                    reparsed.tt_tdb_pointer = tentative;
                    reparsed.has_tt_tdb = 1;
                }
                /* Newly visible TT-TDB triple invariant: the classic 2856-byte
                 * pre-parse always truncates the 24-byte tail at 2856
                 * (static assert at file scope: 2856+24 > 2856), so a triple
                 * that appears only in a wider window is genuine header
                 * metadata, never constants-record bytes. No runtime guard
                 * needed; a 2856-wide candidate inventing a triple still
                 * fails the width==classic case above. */
                attempt_bytes = width;
                split_bytes[ei] = attempt_bytes;
                split_totals[ei] = attempt_total;
                split_headers[ei] = reparsed;
                split_ok[ei] = 1;
            }
        }
        /* At most one split may validate: both splits validated means
         * the file admits two physical readings (different totals, so
         * necessarily different record counts) and is materially
         * ambiguous (R06b) -> FORMAT. A plain valid file validates
         * exactly one split. */
        if (split_ok[0] && split_ok[1]) return JPL_ERROR_FORMAT;
        if (split_ok[0] && !split_ok[1]) {
            candidate = split_headers[0];
            record_bytes = split_bytes[0];
            total_records = split_totals[0];
        } else if (split_ok[1] && !split_ok[0]) {
            candidate = split_headers[1];
            record_bytes = split_bytes[1];
            total_records = split_totals[1];
        } else {
            return JPL_ERROR_FORMAT;
        }
    }
    if (record_bytes == 0) return JPL_ERROR_FORMAT;
    candidate.record_double_count = record_bytes / 8;
    *header = candidate;
    *record_count = total_records - 2;
    return JPL_OK;
}

/* Candidates identical in every observable field except byte order
 * collapse to one; any material difference between surviving LE/BE
 * interpretations rejects the file rather than choosing the first
 * divisible width. Byte order itself is excluded: two all-zero-style
 * symmetric headers decode identically under both orders. */
static int headers_equal(const jpl_header *a, const jpl_header *b) {
    size_t i;
    if (a->start_jd[0] != b->start_jd[0] || a->start_jd[1] != b->start_jd[1] ||
        a->end_jd[0] != b->end_jd[0] || a->end_jd[1] != b->end_jd[1] ||
        a->step_days != b->step_days ||
        a->constant_count != b->constant_count ||
        a->astronomical_unit_km != b->astronomical_unit_km ||
        a->earth_moon_mass_ratio != b->earth_moon_mass_ratio ||
        a->ephemeris_number != b->ephemeris_number ||
        a->reference_frame != b->reference_frame ||
        a->record_double_count != b->record_double_count ||
        a->has_tt_tdb != b->has_tt_tdb) return 0;
    for (i = 0; i < JPL_TARGET_COUNT; ++i) {
        if (a->pointers[i].start_index != b->pointers[i].start_index ||
            a->pointers[i].coefficients_per_component != b->pointers[i].coefficients_per_component ||
            a->pointers[i].subintervals != b->pointers[i].subintervals) return 0;
    }
    if (a->libration_pointer.start_index != b->libration_pointer.start_index ||
        a->libration_pointer.coefficients_per_component != b->libration_pointer.coefficients_per_component ||
        a->libration_pointer.subintervals != b->libration_pointer.subintervals) return 0;
    if (a->tt_tdb_pointer.start_index != b->tt_tdb_pointer.start_index ||
        a->tt_tdb_pointer.coefficients_per_component != b->tt_tdb_pointer.coefficients_per_component ||
        a->tt_tdb_pointer.subintervals != b->tt_tdb_pointer.subintervals) return 0;
    if (memcmp(a->titles, b->titles, sizeof a->titles) != 0) return 0;
    if (memcmp(a->constant_names, b->constant_names, sizeof a->constant_names) != 0) return 0;
    return 1;
}

/* Evaluate LE and BE candidates through the shared validator. Parses
 * into candidate-local headers, never the published header. Returns the
 * unique surviving interpretation, or FORMAT when none (or several
 * different ones) survive; I/O and allocation failures propagate. */
static jpl_status select_layout(const jpl_byte_source *source,
                                jpl_header *header, size_t *record_count) {
    static const jpl_byte_order orders[2] = {
        JPL_BYTE_ORDER_LITTLE_ENDIAN, JPL_BYTE_ORDER_BIG_ENDIAN
    };
    jpl_header survivors[2];
    size_t survivor_counts[2], survivor_total = 0;
    int memory_failure = 0, io_failure = 0, argument_failure = 0;
    size_t o;
    if (!source || !header || !record_count) return JPL_ERROR_ARGUMENT;
    for (o = 0; o < 2; ++o) {
        jpl_header candidate;
        size_t count = 0;
        jpl_status status;
        memset(&candidate, 0, sizeof candidate);
        status = validate_layout(source, orders[o], &candidate, &count);
        if (status == JPL_OK) {
            if (survivor_total < 2) {
                survivors[survivor_total] = candidate;
                survivor_counts[survivor_total] = count;
            }
            ++survivor_total;
        } else if (status == JPL_ERROR_MEMORY) {
            memory_failure = 1;
        } else if (status == JPL_ERROR_IO) {
            io_failure = 1;
        } else if (status == JPL_ERROR_ARGUMENT) {
            argument_failure = 1;
        }
    }
    if (memory_failure && survivor_total == 0) return JPL_ERROR_MEMORY;
    if (io_failure && survivor_total == 0) return JPL_ERROR_IO;
    if (argument_failure && survivor_total == 0 && !io_failure && !memory_failure)
        return JPL_ERROR_ARGUMENT;
    if (survivor_total == 0) return JPL_ERROR_FORMAT;
    if (survivor_total == 2) {
        if (survivor_counts[0] == survivor_counts[1] &&
            headers_equal(&survivors[0], &survivors[1])) {
            /* Same geometry under both orders (e.g. symmetric zeros):
             * report the first order. */
            *header = survivors[0];
            *record_count = survivor_counts[0];
            return JPL_OK;
        }
        return JPL_ERROR_FORMAT;
    }
    if (survivor_total == 1) {
        *header = survivors[0];
        *record_count = survivor_counts[0];
        return JPL_OK;
    }
    /* Defensive: survivor_total cannot exceed 2 (two byte orders), so any
     * other count is an internal logic error, reported as FORMAT. */
    return JPL_ERROR_FORMAT;
}

static uint32_t read_u32(const unsigned char *p, jpl_byte_order order) {
    if (order == JPL_BYTE_ORDER_BIG_ENDIAN)
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

static double read_f64(const unsigned char *p, jpl_byte_order order) {
    uint64_t bits = 0;
    size_t i;
    for (i = 0; i < 8; ++i) {
        size_t source = order == JPL_BYTE_ORDER_BIG_ENDIAN ? i : 7 - i;
        bits = (bits << 8) | p[source];
    }
    { double value; memcpy(&value, &bits, sizeof value); return value; }
}

/* Long-span ephemerides such as DE431/DE441 extend thousands of years either
 * side of the present, so do not restrict a Julian date to modern eras. */
static int plausible_date(double value) { return value > JPL_PLAUSIBLE_JD_MIN && value < JPL_PLAUSIBLE_JD_MAX; }

static int seek_to(FILE *file, size_t offset) {
    /* On 64-bit size_t the offset can exceed the file-offset range;
     * on 32-bit size_t every value fits, so no comparison is needed
     * (and comparing would be vacuous). */
#if SIZE_MAX > INT64_MAX
    if ((uintmax_t)offset > (uintmax_t)INT64_MAX) return -1;
#endif
    return JPL_FSEEK(file, (jpl_file_offset)offset, SEEK_SET);
}

static int checked_record_offset(size_t index, size_t record_doubles, size_t *out) {
    size_t width;
    if (record_doubles == 0 || record_doubles > SIZE_MAX / 8) return -1;
    if (index > SIZE_MAX - 2) return -1;
    width = record_doubles * 8;
    if (index + 2 > SIZE_MAX / width) return -1;
    *out = (index + 2) * width;
    return 0;
}

static void copy_text(char *destination, const unsigned char *source, size_t length) {
    memcpy(destination, source, length);
    while (length > 0 && (destination[length - 1] == ' ' || destination[length - 1] == '\0')) --length;
    destination[length] = '\0';
}

static jpl_status parse_header(jpl_header *header, const unsigned char *bytes, size_t size,
                               jpl_byte_order order) {
    size_t i, maximum = 2;
    const unsigned char *p = bytes;
    if (size < JPL_TT_TDB_TAIL_BASE) return JPL_ERROR_FORMAT;
    memset(header, 0, sizeof *header);
    header->byte_order = order;
    for (i = 0; i < 3; ++i) copy_text(header->titles[i], p + i * 84, 84);
    p += JPL_HDR_TITLES_LEN;
    for (i = 0; i < JPL_MAX_CONSTANT_NAMES; ++i) copy_text(header->constant_names[i], p + i * 6, 6);
    p += JPL_HDR_TEXT_LEN;
    { double enc_start = read_f64(p, order); p += 8;
      double enc_end = read_f64(p, order); p += 8;
      header->start_jd[0] = enc_start; header->start_jd[1] = 0.0;
      header->end_jd[0] = enc_end; header->end_jd[1] = 0.0; }
    header->step_days = read_f64(p, order); p += 8;
    header->constant_count = 0;
    { uint32_t u_nconst = read_u32(p, order); p += 4; if (u_nconst > INT_MAX) return JPL_ERROR_FORMAT; header->constant_count = (int)u_nconst; }
    header->astronomical_unit_km = read_f64(p, order); p += 8;
    header->earth_moon_mass_ratio = read_f64(p, order); p += 8;
    if (!isfinite(header->start_jd[0]) || !isfinite(header->start_jd[1]) ||
        !isfinite(header->end_jd[0]) || !isfinite(header->end_jd[1]) ||
        !plausible_date(header->start_jd[0]) || !plausible_date(header->end_jd[0]) ||
        header->end_jd[0] <= header->start_jd[0] ||
        !isfinite(header->step_days) || header->step_days <= 0.0 ||
        !isfinite(header->astronomical_unit_km) || header->astronomical_unit_km <= 0.0 ||
        !isfinite(header->earth_moon_mass_ratio) || header->earth_moon_mass_ratio <= 0.0)
        return JPL_ERROR_FORMAT;
    for (i = 0; i < JPL_TARGET_COUNT; ++i) {
        jpl_pointer *pointer = &header->pointers[i];
        uint32_t u_start = read_u32(p, order); p += 4;
        uint32_t u_coeff = read_u32(p, order); p += 4;
        uint32_t u_sub = read_u32(p, order); p += 4;
        if (u_start > INT_MAX || u_coeff > INT_MAX || u_sub > INT_MAX) return JPL_ERROR_FORMAT;
        pointer->start_index = (int)u_start;
        pointer->coefficients_per_component = (int)u_coeff;
        pointer->subintervals = (int)u_sub;
        if (pointer->start_index > 0 && pointer->coefficients_per_component > 0 && pointer->subintervals > 0) {
            /* IPT's final entry is nutation, which has longitude and obliquity
             * rather than the three coordinates used by planetary entries. */
            size_t component_count = i == JPL_TARGET_COUNT - 1 ? 2u : 3u;
            size_t cap = SIZE_MAX / component_count / (size_t)pointer->subintervals;
            if (cap == 0 || (size_t)pointer->coefficients_per_component >= cap) return JPL_ERROR_FORMAT;
            size_t product = component_count * (size_t)pointer->coefficients_per_component * (size_t)pointer->subintervals;
            size_t end;
            if (product > SIZE_MAX - ((size_t)pointer->start_index - 1)) return JPL_ERROR_FORMAT;
            end = ((size_t)pointer->start_index - 1) + product;
            if (end > maximum) maximum = end;
        }
    }
    { uint32_t u_denum = read_u32(p, order); p += 4; if (u_denum > INT_MAX) return JPL_ERROR_FORMAT; header->ephemeris_number = (int)u_denum; header->reference_frame = classify_frame(header->ephemeris_number); }
    { uint32_t u_lstart = read_u32(p, order); p += 4; if (u_lstart > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.start_index = (int)u_lstart; }
    { uint32_t u_lcoeff = read_u32(p, order); p += 4; if (u_lcoeff > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.coefficients_per_component = (int)u_lcoeff; }
    { uint32_t u_lsub = read_u32(p, order); p += 4; if (u_lsub > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.subintervals = (int)u_lsub; }
    if (header->libration_pointer.start_index > 0 && header->libration_pointer.coefficients_per_component > 0 && header->libration_pointer.subintervals > 0) {
        size_t lcap = SIZE_MAX / 3u / (size_t)header->libration_pointer.subintervals;
        if (lcap == 0 || (size_t)header->libration_pointer.coefficients_per_component >= lcap) return JPL_ERROR_FORMAT;
        size_t lproduct = 3u * (size_t)header->libration_pointer.coefficients_per_component * (size_t)header->libration_pointer.subintervals;
        size_t end;
        if (lproduct > SIZE_MAX - ((size_t)header->libration_pointer.start_index - 1)) return JPL_ERROR_FORMAT;
        end = ((size_t)header->libration_pointer.start_index - 1) + lproduct;
        if (end > maximum) maximum = end;
    }
    /* IPT entries 14/15 follow the overflow constant-name block: files
     * with more than 400 numeric constants store the extra 6-character
     * names immediately past the legacy 2856-byte cursor, and the two
     * triplets after them. Entry 15 is the TT-TDB dataset (*t files
     * only); entry 14 is parsed for symmetry and ignored. T#16 has no
     * header slot and is not supported. The TT-TDB triple is tentative
     * until record geometry is known (see validate_layout): it is
     * stored raw with has_tt_tdb == 0 and never enlarges
     * record_double_count here. An irrelevant T#14 word never
     * invalidates a separately valid T#15 triple. In-record random
     * bytes that happen to form a valid in-record triple cannot be
     * distinguished from genuine metadata by geometry alone. */
    { size_t extra = header->constant_count > JPL_MAX_CONSTANT_NAMES ?
                         (size_t)(header->constant_count - JPL_MAX_CONSTANT_NAMES) : 0;
      size_t tail;
      uint32_t u_t15s, u_t15c, u_t15l;
      const unsigned char *q;
      header->record_double_count = maximum;
      header->byte_order = order;
      header->has_tt_tdb = 0;
      memset(&header->tt_tdb_pointer, 0, sizeof header->tt_tdb_pointer);
      if (extra > (SIZE_MAX - JPL_TT_TDB_TAIL_BASE) / JPL_CONSTANT_NAME_LENGTH) return JPL_OK;
      tail = JPL_TT_TDB_TAIL_BASE + extra * JPL_CONSTANT_NAME_LENGTH;
      if (tail > size || size - tail < 24) return JPL_OK;
      q = bytes + tail + 12;
      u_t15s = read_u32(q, order); q += 4; u_t15c = read_u32(q, order); q += 4; u_t15l = read_u32(q, order);
      if (u_t15s > INT_MAX || u_t15c > INT_MAX || u_t15l > INT_MAX) return JPL_OK;
      header->tt_tdb_pointer.start_index = (int)u_t15s;
      header->tt_tdb_pointer.coefficients_per_component = (int)u_t15c;
      header->tt_tdb_pointer.subintervals = (int)u_t15l;
      return JPL_OK; }
}

/* Frame classification from the content-derived DENUM (T12/R04).
 * Explicit cited allowlist from JPL's export information
 * (https://ssd.jpl.nasa.gov/planets/eph_export.html): DE102 is referred
 * to the dynamical equator/equinox of 1950, DE200/DE202 to the dynamical
 * equator/equinox of 2000, and the listed modern releases are
 * ICRF-aligned. All other values stay UNKNOWN but remain readable by
 * core native-frame APIs; ICRS convenience requires ICRF. */
static jpl_reference_frame classify_frame(int denum) {
    switch (denum) {
    case 102: return JPL_FRAME_DYNAMICAL_1950;
    case 200: case 202: return JPL_FRAME_DYNAMICAL_2000;
    case 403: case 405: case 406: case 410: case 413:
    case 421: case 422: case 423: case 430: case 431:
    case 432: case 440: case 441: return JPL_FRAME_ICRF;
    default: return JPL_FRAME_UNKNOWN;
    }
}

static jpl_byte_order native_order(void) {
    const uint16_t probe = 1;
    return *(const unsigned char *)&probe ? JPL_BYTE_ORDER_LITTLE_ENDIAN : JPL_BYTE_ORDER_BIG_ENDIAN;
}

static jpl_status read_doubles(const jpl_ephemeris *ephemeris, size_t offset, double *values, size_t count) {
    unsigned char *bytes; size_t i;
    if (ephemeris->is_mmap) {
        const unsigned char *base;
        if (!ephemeris->mmap_base) return JPL_ERROR_IO;
        if (count > SIZE_MAX / 8) return JPL_ERROR_IO;
        if (offset > ephemeris->mmap_size || count * 8 > ephemeris->mmap_size - offset) return JPL_ERROR_IO;
        base = ephemeris->mmap_base + offset;
        if (ephemeris->header.byte_order == native_order()) { memcpy(values, base, count * 8); return JPL_OK; }
        for (i = 0; i < count; ++i) values[i] = read_f64(base + i * 8, ephemeris->header.byte_order);
        return JPL_OK;
    }
    if (count > SIZE_MAX / 8 || seek_to(ephemeris->file, offset) != 0) return JPL_ERROR_IO;
    if (ephemeris->header.byte_order == native_order())
        return fread(values, 8, count, ephemeris->file) == count ? JPL_OK : JPL_ERROR_IO;
    bytes = malloc(count * 8); if (!bytes) return JPL_ERROR_MEMORY;
    if (fread(bytes, 8, count, ephemeris->file) != count) { free(bytes); return JPL_ERROR_IO; }
    for (i = 0; i < count; ++i) values[i] = read_f64(bytes + i * 8, ephemeris->header.byte_order);
    free(bytes); return JPL_OK;
}

const char *jpl_status_string(jpl_status status) { static const char *const text[] = { "success", "invalid argument", "I/O error", "invalid JPL DE format", "out of range", "out of memory", "unsupported dataset or frame", "solution did not converge", "unrepresentable numeric result" }; return status >= JPL_OK && status <= JPL_ERROR_NUMERIC ? text[status] : "unknown error"; }

jpl_status jpl_open(const char *path, jpl_ephemeris **out) {
    jpl_ephemeris *e; jpl_status status;
    jpl_byte_source source;
    jpl_file_source fsource;
    char *resolved = NULL;
    if (!path || !out) return JPL_ERROR_ARGUMENT;
    *out = NULL;
    e = calloc(1, sizeof *e); if (!e) return JPL_ERROR_MEMORY;
    e->refcount = malloc(sizeof *e->refcount); if (!e->refcount) { free(e); return JPL_ERROR_MEMORY; }
    refcount_init(e->refcount);
    e->is_mmap = 0; e->mmap_base = NULL; e->mmap_size = 0;
    /* Resolve the absolute reopen path before fopen and open that exact
     * resolved path, so a later cwd change cannot redirect clones. */
    resolved = resolve_reopen_path(path);
    if (!resolved) { free(e->refcount); free(e); return JPL_ERROR_MEMORY; }
    e->path = resolved;
    e->file = fopen(resolved, "rb"); if (!e->file) { jpl_close(e); return JPL_ERROR_IO; }
    if (JPL_FSEEK(e->file, 0, SEEK_END) != 0) { jpl_close(e); return JPL_ERROR_IO; }
    { jpl_file_offset end = JPL_FTELL(e->file); if (end < 0 || (uintmax_t)end > SIZE_MAX) { jpl_close(e); return JPL_ERROR_IO; } e->file_size = (size_t)end; }
    /* Capture identity from the open stream (not a path lookup). */
    if (capture_stream_identity(e->file, e) != 0) { jpl_close(e); return JPL_ERROR_IO; }
    /* One shared validator for FILE and mmap: bounded reads go through
     * the byte source, so both paths evaluate complete LE/BE layout
     * candidates instead of accepting a preliminary byte order. */
    fsource.file = e->file;
    fsource.size = e->file_size;
    source.context = &fsource;
    source.size = e->file_size;
    source.read = file_read_bytes;
    status = select_layout(&source, &e->header, &e->record_count);
    if (status != JPL_OK) { jpl_status keep = status; jpl_close(e); return keep == JPL_ERROR_MEMORY ? JPL_ERROR_MEMORY : (keep == JPL_ERROR_IO ? JPL_ERROR_IO : JPL_ERROR_FORMAT); }
    if (e->header.record_double_count == 0 || e->header.record_double_count > SIZE_MAX / 8) { jpl_close(e); return JPL_ERROR_FORMAT; }
    *out = e; return JPL_OK;
}

void jpl_close(jpl_ephemeris *e) {
    if (!e) return;
    if (!e->refcount) { /* Partially constructed handle: best-effort release. */
        if (e->file) fclose(e->file);
        free(e->path);
        free(e->cache_record);
        free(e);
        return;
    }
    /* refcount_dec returns the remaining count; siblings keep the mapping.
     * Each handle frees its own cache_record: the cache is per-handle, so
     * a closing sibling must not orphan its record buffer. */
    if (refcount_dec(e->refcount) > 0) { free(e->cache_record); free(e); return; }
    if (e->is_mmap) {
#if defined(_WIN32)
        if (e->mmap_base) UnmapViewOfFile(e->mmap_base);
        if (e->hMap) CloseHandle(e->hMap);
        if (e->hFile && e->hFile != INVALID_HANDLE_VALUE) CloseHandle(e->hFile);
#else
        if (e->mmap_base && e->mmap_size > 0) munmap(e->mmap_base, e->mmap_size);
#endif
    } else {
        if (e->file) fclose(e->file);
    }
    free(e->refcount);
    free(e->path);
    free(e->cache_record);
    free(e);
}

jpl_status jpl_open_mmap(const char *path, jpl_ephemeris **out) {
    jpl_ephemeris *e;
    jpl_header header;
    jpl_status status;
    size_t file_size = 0, record_count = 0;
    if (!path || !out) return JPL_ERROR_ARGUMENT;
    *out = NULL;
#if defined(_WIN32)
    {
        HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE hMap = NULL;
        unsigned char *base = NULL;
        LARGE_INTEGER size;
        if (hFile == INVALID_HANDLE_VALUE) return JPL_ERROR_IO;
        if (!GetFileSizeEx(hFile, &size) || size.QuadPart < (LONGLONG)MIN_RECORD_BYTES ||
            (uintmax_t)size.QuadPart > (uintmax_t)SIZE_MAX) { CloseHandle(hFile); return JPL_ERROR_IO; }
        file_size = (size_t)size.QuadPart;
        hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMap) { CloseHandle(hFile); return JPL_ERROR_IO; }
        /* Re-check size after mapping: the file must not change under us
         * (truncation after mapping faults on access; growth is ignored). */
        {
            LARGE_INTEGER resize;
            if (!GetFileSizeEx(hFile, &resize) || resize.QuadPart != (LONGLONG)file_size) {
                CloseHandle(hMap); CloseHandle(hFile); return JPL_ERROR_IO;
            }
        }
        base = (unsigned char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (!base) { CloseHandle(hMap); CloseHandle(hFile); return JPL_ERROR_IO; }
        /* Bounded mmap reads through the shared candidate validator. */
        {
            jpl_byte_source source;
            jpl_mmap_source msource;
            msource.base = base;
            msource.size = file_size;
            source.context = &msource;
            source.size = file_size;
            source.read = mmap_read_bytes;
            status = select_layout(&source, &header, &record_count);
            if (status == JPL_OK &&
                (header.record_double_count == 0 || header.record_double_count > SIZE_MAX / 8))
                status = JPL_ERROR_FORMAT;
        }
        if (status != JPL_OK) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return status == JPL_ERROR_MEMORY ? JPL_ERROR_MEMORY : (status == JPL_ERROR_IO ? JPL_ERROR_IO : JPL_ERROR_FORMAT); }
        e = calloc(1, sizeof *e);
        if (!e) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return JPL_ERROR_MEMORY; }
        e->refcount = malloc(sizeof *e->refcount);
        if (!e->refcount) { free(e); UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return JPL_ERROR_MEMORY; }
        refcount_init(e->refcount);
        e->is_mmap = 1;
        e->mmap_base = base;
        e->mmap_size = file_size;
        e->hFile = hFile;
        e->hMap = hMap;
        e->header = header;
        e->file_size = file_size;
        e->record_count = record_count;
        *out = e;
        return JPL_OK;
    }
#else
    {
        int fd = open(path, O_RDONLY);
        struct stat st;
        unsigned char *base = NULL;
        if (fd < 0) return JPL_ERROR_IO;
        if (fstat(fd, &st) != 0 || st.st_size < (off_t)MIN_RECORD_BYTES || (uintmax_t)st.st_size > (uintmax_t)SIZE_MAX) {
            close(fd);
            return JPL_ERROR_IO;
        }
        file_size = (size_t)st.st_size;
        base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (base == MAP_FAILED) { close(fd); return JPL_ERROR_IO; }
        /* Re-check size on the SAME fd after mapping: renames/replacements
         * of path no longer validate the wrong file. Truncation after
         * mapping can still fault on access (documented). */
        {
            struct stat rest;
            if (fstat(fd, &rest) != 0 || rest.st_size != st.st_size) {
                close(fd); munmap(base, file_size); return JPL_ERROR_IO;
            }
        }
        close(fd);
        {
            jpl_byte_source source;
            jpl_mmap_source msource;
            msource.base = base;
            msource.size = file_size;
            source.context = &msource;
            source.size = file_size;
            source.read = mmap_read_bytes;
            status = select_layout(&source, &header, &record_count);
            if (status == JPL_OK &&
                (header.record_double_count == 0 || header.record_double_count > SIZE_MAX / 8))
                status = JPL_ERROR_FORMAT;
        }
        if (status != JPL_OK) { munmap(base, file_size); return status == JPL_ERROR_MEMORY ? JPL_ERROR_MEMORY : (status == JPL_ERROR_IO ? JPL_ERROR_IO : JPL_ERROR_FORMAT); }
        e = calloc(1, sizeof *e);
        if (!e) { munmap(base, file_size); return JPL_ERROR_MEMORY; }
        e->refcount = malloc(sizeof *e->refcount);
        if (!e->refcount) { free(e); munmap(base, file_size); return JPL_ERROR_MEMORY; }
        refcount_init(e->refcount);
        e->is_mmap = 1;
        e->mmap_base = base;
        e->mmap_size = file_size;
        e->header = header;
        e->file_size = file_size;
        e->record_count = record_count;
        *out = e;
        return JPL_OK;
    }
#endif
}

jpl_status jpl_ephemeris_clone(const jpl_ephemeris *ephem, jpl_ephemeris **out_clone) {
    jpl_ephemeris *c;
    if (!out_clone) return JPL_ERROR_ARGUMENT;
    *out_clone = NULL;
    if (!ephem) return JPL_ERROR_ARGUMENT;
    c = calloc(1, sizeof *c);
    if (!c) return JPL_ERROR_MEMORY;
    c->header = ephem->header;
    c->file_size = ephem->file_size;
    c->record_count = ephem->record_count;
    c->is_mmap = ephem->is_mmap;
    if (ephem->is_mmap) {
        if (!ephem->refcount) { free(c); return JPL_ERROR_ARGUMENT; }
        c->refcount = ephem->refcount;
        c->mmap_base = ephem->mmap_base;
        c->mmap_size = ephem->mmap_size;
#if defined(_WIN32)
        c->hFile = ephem->hFile;
        c->hMap = ephem->hMap;
#endif
        refcount_inc(c->refcount);
    } else {
        if (!ephem->path) { free(c); return JPL_ERROR_ARGUMENT; }
        c->refcount = malloc(sizeof *c->refcount);
        if (!c->refcount) { free(c); return JPL_ERROR_MEMORY; }
        refcount_init(c->refcount);
        c->path = copy_string(ephem->path);
        if (!c->path) { free(c->refcount); free(c); return JPL_ERROR_MEMORY; }
        /* Independent stream at the stored absolute path; identity is
         * verified against the source's live stream before publishing.
         * No POSIX dup: dup shares the file offset, clone needs an
         * independent position. */
        c->file = fopen(ephem->path, "rb");
        if (!c->file) { jpl_close(c); *out_clone = NULL; return JPL_ERROR_IO; }
        if (stream_identity_matches(ephem, c->file) != 0) {
            jpl_close(c);
            *out_clone = NULL;
            return JPL_ERROR_IO;
        }
        c->file_size = ephem->file_size;
        c->record_count = ephem->record_count;
#if !defined(_WIN32)
        c->id_dev = ephem->id_dev;
        c->id_ino = ephem->id_ino;
#else
        c->id_volume = ephem->id_volume;
        c->id_index_hi = ephem->id_index_hi;
        c->id_index_lo = ephem->id_index_lo;
#endif
        /* Cold start: calloc zeroed cache_valid, so this clone never reads
         * the source handle's cached record. */
    }
    *out_clone = c;
    return JPL_OK;
}
const jpl_header *jpl_get_header(const jpl_ephemeris *e) { return e ? &e->header : NULL; }
size_t jpl_record_count(const jpl_ephemeris *e) { return e ? e->record_count : 0; }
int jpl_has_tt_tdb(const jpl_ephemeris *e) { return e ? e->header.has_tt_tdb : 0; }
jpl_status jpl_read_constants(const jpl_ephemeris *e, double *values, size_t count) {
    size_t offset;
    if (!e || !values || count < (size_t)e->header.constant_count) return JPL_ERROR_ARGUMENT;
    if (e->header.record_double_count > SIZE_MAX / 8) return JPL_ERROR_IO;
    offset = e->header.record_double_count * 8;
    return read_doubles(e, offset, values, (size_t)e->header.constant_count);
}
jpl_status jpl_read_record(const jpl_ephemeris *e, size_t index, double *values, size_t count) {
    size_t offset;
    if (!e || !values || count < e->header.record_double_count) return JPL_ERROR_ARGUMENT;
    if (index >= e->record_count) return JPL_ERROR_RANGE;
    if (checked_record_offset(index, e->header.record_double_count, &offset) != 0) return JPL_ERROR_IO;
    return read_doubles(e, offset, values, e->header.record_double_count);
}

/* Locate the data record holding a normalized query date.
 * A bounded candidate quotient from the compensated coverage difference
 * estimates the index; the candidate's reconstructed boundaries (via
 * add_scaled/partition) verify it against the full pair, with at most a
 * one-neighbor adjustment, then a bounded binary search fallback over the
 * verified intervals (never an unbounded walk on malformed data). Interior
 * boundaries select the following interval; the final advertised coverage
 * endpoint explicitly selects the last advertised interval. */
static jpl_status select_record(const jpl_ephemeris *e, const double jd[2],
                                size_t *index_out,
                                double record_start[2], double record_end[2]) {
    double cov_start[2], norm[2], diff[2], boundary[2], next[2];
    double quotient_d;
    size_t candidate = 0, lo, hi;
    jpl_status status;
    if (!e || !jd || !index_out || !record_start || !record_end) return JPL_ERROR_ARGUMENT;
    if (e->record_count == 0) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(e->header.start_jd, cov_start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (!isfinite(e->header.step_days) || e->header.step_days <= 0.0) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(jd, norm);
    if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
    /* Final advertised endpoint maps to the last covered interval. */
    status = jpl_date_normalize(e->header.end_jd, boundary);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(norm, boundary) == 0) {
        candidate = e->record_count - 1;
        status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate, record_start);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        *index_out = candidate;
        record_end[0] = boundary[0];
        record_end[1] = boundary[1];
        return JPL_OK;
    }
    status = jpl_date_subtract(norm, cov_start, diff);
    if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
    /* Scalar span/quotient/normalized math: record spans are whole-day
     * values, so collapsing the compensated two-part difference
     * (rec_span/from_start/local from jpl_date_subtract) into one scalar
     * for the rate divisor, candidate quotient, or Chebyshev argument is
     * a small-offset candidate/rate computation. The inputs are already
     * FINITE normalized pairs; nonfinite quotients (NUMERIC) and
     * nonpositive spans map to FORMAT/RANGE as checked below. */
    quotient_d = (diff[0] + diff[1]) / e->header.step_days;
    if (!isfinite(quotient_d) || quotient_d < 0.0) return JPL_ERROR_RANGE;
    candidate = (size_t)quotient_d;
    if (candidate >= e->record_count) candidate = e->record_count - 1;
    for (;;) {
        int below = 0, above = 0;
        status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate, boundary);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        if (candidate + 1 < e->record_count) {
            status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate + 1, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
        } else {
            status = jpl_date_normalize(e->header.end_jd, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
        }
        below = jpl_date_compare(norm, boundary) < 0;
        above = jpl_date_compare(norm, next) >= 0 && candidate + 1 < e->record_count;
        if (!below && !above) {
            status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate, record_start);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            if (candidate + 1 < e->record_count) {
                status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate + 1, record_end);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            } else {
                status = jpl_date_normalize(e->header.end_jd, record_end);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            }
            *index_out = candidate;
            return JPL_OK;
        }
        /* One-neighbor correction covers rounding at the estimate; larger
         * drift falls through to the bounded binary search below. */
        if (below && candidate > 0) {
            size_t probe = candidate - 1;
            status = jpl_date_add_scaled(cov_start, e->header.step_days, probe, boundary);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            status = jpl_date_add_scaled(cov_start, e->header.step_days, candidate, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            if (jpl_date_compare(norm, boundary) >= 0 && jpl_date_compare(norm, next) < 0) {
                candidate = probe;
                continue;
            }
            break;
        }
        if (above && candidate + 1 < e->record_count) {
            size_t probe = candidate + 1;
            status = jpl_date_add_scaled(cov_start, e->header.step_days, probe, boundary);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            if (candidate + 2 < e->record_count) {
                status = jpl_date_add_scaled(cov_start, e->header.step_days, probe + 1, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            } else {
                status = jpl_date_normalize(e->header.end_jd, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            }
            if (jpl_date_compare(norm, boundary) >= 0 &&
                (jpl_date_compare(norm, next) < 0 || probe + 1 >= e->record_count)) {
                candidate = probe;
                continue;
            }
            break;
        }
        break;
    }
    /* Bounded binary search over verified intervals. */
    lo = 0;
    hi = e->record_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        status = jpl_date_add_scaled(cov_start, e->header.step_days, mid, boundary);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        if (jpl_date_compare(norm, boundary) < 0) {
            hi = mid;
        } else {
            if (mid + 1 < e->record_count) {
                status = jpl_date_add_scaled(cov_start, e->header.step_days, mid + 1, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            } else {
                status = jpl_date_normalize(e->header.end_jd, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            }
            if (jpl_date_compare(norm, next) < 0 || mid + 1 >= e->record_count) {
                status = jpl_date_add_scaled(cov_start, e->header.step_days, mid, record_start);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
                record_end[0] = next[0];
                record_end[1] = next[1];
                *index_out = mid;
                return JPL_OK;
            }
            lo = mid + 1;
        }
    }
    return JPL_ERROR_RANGE;
}

static jpl_status interpolate(const jpl_ephemeris *e, const double jd[2], const jpl_pointer *pointer,
                              size_t components, double state[6]) {
    double *record;
    double norm[2], sched_start[2], sched_end[2], file_start[2], file_end[2];
    double span, substep, normalized;
    double rec_span[2], from_start[2], sub_origin[2], local[2];
    double local_state[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    size_t record_index, subinterval, component, coefficient;
    size_t nsub;
    int cached, use_cache;
    jpl_status status;
    if (!e || !jd || !pointer || !state) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd[0]) || !isfinite(jd[1])) return JPL_ERROR_ARGUMENT;
    status = jpl_date_normalize(jd, norm);
    if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
    /* Coverage precedes the missing-dataset report (TT-TDB path order):
     * valid in-coverage queries for absent data are UNSUPPORTED, while
     * out-of-coverage queries stay RANGE even when the dataset is absent. */
    status = jpl_date_normalize(e->header.start_jd, sched_start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(e->header.end_jd, sched_end);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(norm, sched_start) < 0 || jpl_date_compare(norm, sched_end) > 0)
        return JPL_ERROR_RANGE;
    if (pointer->start_index <= 0 || pointer->coefficients_per_component <= 0 ||
        pointer->subintervals <= 0) return JPL_ERROR_UNSUPPORTED;
    if (e->record_count == 0) return JPL_ERROR_FORMAT;
    status = select_record(e, norm, &record_index, sched_start, sched_end);
    if (status != JPL_OK) return status;
    /* Single-record cache is for the FILE path only: it mutates per-handle
     * state, so the lock-free mmap path bypasses it entirely (reads on a
     * mapped handle perform no per-handle mutation). Light-time loops
     * almost always re-read the same record, hence the FILE-path cache.
     * NOTE: the cache is only replaced after interpolation succeeds;
     * failures leave the prior entry intact. */
    use_cache = !e->is_mmap;
    cached = use_cache && e->cache_valid && e->cache_record && e->cache_index == record_index;
    record = malloc(e->header.record_double_count * sizeof *record);
    if (!record) return JPL_ERROR_MEMORY;
    if (cached) {
        memcpy(record, e->cache_record, e->header.record_double_count * sizeof *record);
    } else {
        status = jpl_read_record(e, record_index, record, e->header.record_double_count);
        if (status != JPL_OK) { free(record); return status; }
    }
    /* Promote the encoded record bounds to pairs at the read boundary, then
     * verify the selected scheduled interval against them: the scheduled
     * endpoints schedule the query, the file endpoints bound it. */
    file_start[0] = record[0]; file_start[1] = 0.0;
    file_end[0] = record[1]; file_end[1] = 0.0;
    status = jpl_date_normalize(file_start, file_start);
    if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
    status = jpl_date_normalize(file_end, file_end);
    if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
    if (jpl_date_compare(file_end, file_start) <= 0) { free(record); return JPL_ERROR_FORMAT; }
    if (jpl_date_compare(norm, file_start) < 0 || jpl_date_compare(norm, file_end) > 0) {
        free(record);
        return JPL_ERROR_FORMAT;
    }
    /* Scalar span/quotient/normalized math: record spans are whole-day
     * values, so collapsing the compensated two-part difference
     * (rec_span/from_start/local from jpl_date_subtract) into one scalar
     * for the rate divisor, candidate quotient, or Chebyshev argument is
     * a small-offset candidate/rate computation. The inputs are already
     * FINITE normalized pairs; nonfinite quotients (NUMERIC) and
     * nonpositive spans map to FORMAT/RANGE as checked below. */
    status = jpl_date_subtract(file_end, file_start, rec_span);
    if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
    span = rec_span[0] + rec_span[1];
    if (!isfinite(span) || span <= 0.0) { free(record); return JPL_ERROR_FORMAT; }
    /* Defensive only: nonpositive subintervals already report UNSUPPORTED
     * at the early pointer check above; keep the same category here. */
    if (pointer->subintervals <= 0) { free(record); return JPL_ERROR_UNSUPPORTED; }
    nsub = (size_t)pointer->subintervals;
    substep = span / (double)nsub;
    if (!isfinite(substep) || substep <= 0.0) { free(record); return JPL_ERROR_FORMAT; }
    status = jpl_date_subtract(norm, file_start, from_start);
    if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
    { double qd = (from_start[0] + from_start[1]) / substep;
      if (!isfinite(qd) || qd < 0.0) { free(record); return JPL_ERROR_FORMAT; }
      subinterval = (size_t)qd;
      if (subinterval >= nsub) subinterval = nsub - 1;
      /* Verify the candidate against two-part subinterval boundaries and
       * correct by one neighbor; interior boundaries select the following
       * subinterval. */
      for (;;) {
          double lo[2], hi[2];
          int below = 0, above = 0;
          status = jpl_date_partition(file_start, file_end, nsub, subinterval, lo);
          if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
          status = jpl_date_partition(file_start, file_end, nsub, subinterval + 1, hi);
          if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
          below = jpl_date_compare(norm, lo) < 0;
          above = subinterval + 1 < nsub ? jpl_date_compare(norm, hi) >= 0 : 0;
          if (!below && !above) break;
          if (below && subinterval > 0) { --subinterval; continue; }
          if (above && subinterval + 1 < nsub) { ++subinterval; continue; }
          free(record);
          return JPL_ERROR_FORMAT;
      }
      status = jpl_date_partition(file_start, file_end, nsub, subinterval, sub_origin);
      if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
      status = jpl_date_subtract(norm, sub_origin, local);
      if (status != JPL_OK) { free(record); return JPL_ERROR_FORMAT; }
      /* Local Chebyshev argument only after compensated subtraction of the
       * selected nearby boundary. */
      normalized = 2.0 * ((local[0] + local[1]) / substep) - 1.0;
      if (!isfinite(normalized)) { free(record); return JPL_ERROR_FORMAT; }
    }
    for (component = 0; component < components; ++component) {
        size_t start = (size_t)pointer->start_index - 1 +
            (subinterval * components + component) * (size_t)pointer->coefficients_per_component;
        double sum, derivative = 0.0;
        if (start + (size_t)pointer->coefficients_per_component > e->header.record_double_count) { free(record); return JPL_ERROR_FORMAT; }
        /* FORMAT: every used coefficient must be finite. The sum seed
         * always reads record[start]; record[start+1] seeds the linear
         * term and derivative, so it is checked only when
         * coefficients_per_component > 1 (no second slot exists when
         * ncoeff == 1). */
        if (!isfinite(record[start])) { free(record); return JPL_ERROR_FORMAT; }
        if (pointer->coefficients_per_component > 1 && !isfinite(record[start + 1])) { free(record); return JPL_ERROR_FORMAT; }
        sum = record[start];
        if (pointer->coefficients_per_component > 1) {
            /* Finite seed times a finite argument can still overflow to
             * Inf; that is NUMERIC, not FORMAT (inputs were finite). */
            sum += record[start + 1] * normalized;
            if (!isfinite(sum)) { free(record); return JPL_ERROR_NUMERIC; }
        }
        /* Re-evaluate derivative with the standard Chebyshev recurrence.
         * All recurrence inputs are finite by construction (finite
         * coefficients, finite normalized checked above), so guard each
         * stage: a nonfinite coefficient read is FORMAT, while overflow
         * from finite values is NUMERIC. */
        if (pointer->coefficients_per_component > 1) {
            double t0 = 1.0, t1 = normalized, d0 = 0.0, d1 = 1.0;
            derivative = record[start + 1];
            if (!isfinite(derivative)) { free(record); return JPL_ERROR_FORMAT; }
            for (coefficient = 2; coefficient < (size_t)pointer->coefficients_per_component; ++coefficient) {
                double t2 = 2.0 * normalized * t1 - t0;
                double d2 = 2.0 * t1 + 2.0 * normalized * d1 - d0;
                double add;
                if (!isfinite(record[start + coefficient])) { free(record); return JPL_ERROR_FORMAT; }
                if (!isfinite(t2) || !isfinite(d2)) { free(record); return JPL_ERROR_NUMERIC; }
                add = record[start + coefficient] * t2;
                if (!isfinite(add)) { free(record); return JPL_ERROR_NUMERIC; }
                sum += add;
                if (!isfinite(sum)) { free(record); return JPL_ERROR_NUMERIC; }
                add = record[start + coefficient] * d2;
                if (!isfinite(add)) { free(record); return JPL_ERROR_NUMERIC; }
                derivative += add;
                if (!isfinite(derivative)) { free(record); return JPL_ERROR_NUMERIC; }
                t0 = t1; t1 = t2; d0 = d1; d1 = d2;
            }
        }
        /* Transactional locals: the caller's state[] is published only
         * after every component passes (see the commit loop below). */
        local_state[component] = sum;
        local_state[component + 3] = derivative * 2.0 / substep;
        if (!isfinite(local_state[component]) || !isfinite(local_state[component + 3])) { free(record); return JPL_ERROR_NUMERIC; }
    }
    /* Nutations interpolate 2 components; zero the unused third slot. */
    for (; component < 3; ++component) { local_state[component] = 0.0; local_state[component + 3] = 0.0; }
    memcpy(state, local_state, sizeof local_state);
    /* Refresh the single-record cache only on the FILE path, and only now
     * that interpolation succeeded. Best-effort: a failed cache malloc
     * must not discard good state. On failure paths above the function
     * returns early, so the prior entry is never invalidated by a miss. */
    if (!use_cache) { free(record); return JPL_OK; }
    if (!((jpl_ephemeris *)e)->cache_record) {
        ((jpl_ephemeris *)e)->cache_record = malloc(e->header.record_double_count * sizeof *record);
        if (!((jpl_ephemeris *)e)->cache_record) { free(record); return JPL_OK; }
    }
    memcpy(((jpl_ephemeris *)e)->cache_record, record, e->header.record_double_count * sizeof *record);
    ((jpl_ephemeris *)e)->cache_index = record_index;
    ((jpl_ephemeris *)e)->cache_valid = 1;
    free(record);
    return JPL_OK;
}

static jpl_status barycentric_state(const jpl_ephemeris *e, const double jd[2], int target, double state[6]) {
    double moon[6], local[6]; size_t i; jpl_status status;
    double norm[2], cov_start[2], cov_end[2];
    double divisor;
    /* SSB-to-SSB dates validate normally: normalize and check coverage
     * before returning the zero vector (L03). */
    if (!e || !jd || !state) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd[0]) || !isfinite(jd[1])) return JPL_ERROR_ARGUMENT;
    status = jpl_date_normalize(jd, norm);
    if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
    status = jpl_date_normalize(e->header.start_jd, cov_start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(e->header.end_jd, cov_end);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(norm, cov_start) < 0 || jpl_date_compare(norm, cov_end) > 0)
        return JPL_ERROR_RANGE;
    if (target == JPL_SOLAR_SYSTEM_BARYCENTER || target == JPL_SSB) { memset(state, 0, 6 * sizeof *state); return JPL_OK; }
    /* Direct file slots cover Mercury..Sun except Earth (3, derived below)
     * and the Moon (10, needs the EMB leg too). */
    if (target >= JPL_MERCURY && target <= JPL_SUN && target != JPL_MOON && target != JPL_EARTH)
        return interpolate(e, jd, &e->header.pointers[target - 1], 3, state);
    if (target != JPL_EARTH && target != JPL_MOON && target != JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
    status = interpolate(e, jd, &e->header.pointers[JPL_FILE_SLOT_EMB], 3, state);
    if (status != JPL_OK) return status;
    if (target == JPL_EARTH_MOON_BARYCENTER) return JPL_OK;
    status = interpolate(e, jd, &e->header.pointers[JPL_MOON - 1], 3, moon);
    if (status != JPL_OK) return status;
    /* Earth/Moon reconstruction in local storage: every accumulated and
     * final state must be finite. The header EMRAT is validated at open,
     * but re-check the divisor here so a mutated in-memory header cannot
     * produce a silent Inf. */
    divisor = 1.0 + e->header.earth_moon_mass_ratio;
    if (!isfinite(divisor) || divisor == 0.0) return JPL_ERROR_FORMAT;
    for (i = 0; i < 6; ++i) {
        double leg = moon[i] / divisor;
        if (!isfinite(leg)) return JPL_ERROR_NUMERIC;
        local[i] = state[i] - leg;
        if (!isfinite(local[i])) return JPL_ERROR_NUMERIC;
    }
    if (target == JPL_MOON) {
        for (i = 0; i < 6; ++i) {
            local[i] += moon[i];
            if (!isfinite(local[i])) return JPL_ERROR_NUMERIC;
        }
    }
    memcpy(state, local, sizeof local);
    return JPL_OK;
}

jpl_status jpl_get_state(const jpl_ephemeris *e, const double jd[2], int target, int center, double state[6]) {
    double target_state[6], center_state[6], local[6]; size_t i; jpl_status status;
    if (!e || !state || !jd) return JPL_ERROR_ARGUMENT;
    /* Target zero is consistently invalid; center zero is the SSB alias
     * (L05). Validate target before any shortcut. */
    if (target == JPL_SSB) return JPL_ERROR_ARGUMENT;
    if (target == JPL_NUTATIONS || target == JPL_LIBRATIONS) {
        if (center != 0) return JPL_ERROR_ARGUMENT;
        /* Angular getters route through transactional local output too:
         * interpolate validates and publishes atomically, so a bad
         * second/third component cannot leave partial caller output. */
        return interpolate(e, jd, target == JPL_NUTATIONS ? &e->header.pointers[JPL_TARGET_COUNT - 1] : &e->header.libration_pointer,
                           target == JPL_NUTATIONS ? 2 : 3, state);
    }
    /* Body codes 1-13 plus center 0/12 route through barycentric legs
     * below; anything else is rejected after date validation inside
     * barycentric_state. Validate pointers/dates first so a missing
     * dataset reports UNSUPPORTED only after valid checks pass. */
    status = barycentric_state(e, jd, target, target_state);
    if (status != JPL_OK) return status;
    if (center == 0) memset(center_state, 0, sizeof center_state);
    else { status = barycentric_state(e, jd, center, center_state); if (status != JPL_OK) return status; }
    /* Relative state plus modern-AU scaling in local storage: validate
     * every accumulated/scaled component before publishing. */
    for (i = 0; i < 6; ++i) {
        double diff = target_state[i] - center_state[i];
        if (!isfinite(diff)) return JPL_ERROR_NUMERIC;
        local[i] = diff / JPL_AU_KM;
        if (!isfinite(local[i])) return JPL_ERROR_NUMERIC;
    }
    memcpy(state, local, sizeof local);
    return JPL_OK;
}

/* Light-time residual evaluation at one candidate emission epoch.
 * The observer barycentric leg stays fixed at reception (raw file units,
 * km); only the target leg is evaluated at the candidate emission epoch.
 * Returns the relative state in AU in candidate_rel, the geometric delay
 * (range/c, days) in *delay_out, and residual = (receive - emission) -
 * delay in *residual_out. The elapsed term uses compensated two-part
 * subtraction, so the large upper-delay endpoint never collapses the
 * endpoint residual into a rounded scalar. Velocity slots follow the
 * documented mixed-epoch formula v_target(emission) - v_center(reception):
 * they are not the derivative of apparent position with respect to
 * reception time. The file's evaluated barycentric target velocity must
 * satisfy the documented subluminal-motion contract; a superluminal file
 * state is nonphysical and reported as FORMAT, never used to claim
 * bracket uniqueness. */
static jpl_status lighttime_evaluate(const jpl_ephemeris *e, int target,
                                     const double receive[2], const double emission[2],
                                     const double observer_km[6],
                                     double candidate_rel[6], double *delay_out,
                                     double *residual_out) {
    double tgt_km[6], diff[2], elapsed, range_au, delay, residual;
    size_t k;
    jpl_status st = barycentric_state(e, emission, target, tgt_km);
    if (st != JPL_OK) return st;
    /* Subluminal contract on the evaluated barycentric target velocity.
     * Barycentric legs are raw file units per day (coefficients in km,
     * span in days), so convert the velocity norm by the modern AU before
     * comparing against light speed in AU/day. A nonphysical file state
     * is FORMAT, never a uniqueness claim for the bracket. */
    {
        const double vel[3] = { tgt_km[3], tgt_km[4], tgt_km[5] };
        double v_au = jpl_vec_norm3(vel) / JPL_AU_KM;
        if (!isfinite(v_au)) return JPL_ERROR_NUMERIC;
        if (v_au >= JPL_C_AU_PER_DAY) return JPL_ERROR_FORMAT;
    }
    for (k = 0; k < 6; ++k) {
        double leg = tgt_km[k] - observer_km[k];
        if (!isfinite(leg)) return JPL_ERROR_NUMERIC;
        candidate_rel[k] = leg / JPL_AU_KM;
        if (!isfinite(candidate_rel[k])) return JPL_ERROR_NUMERIC;
    }
    range_au = jpl_vec_norm3(candidate_rel);
    if (!isfinite(range_au)) return JPL_ERROR_NUMERIC;
    delay = range_au / JPL_C_AU_PER_DAY;
    if (!isfinite(delay) || delay < 0.0) return JPL_ERROR_NUMERIC;
    st = jpl_date_subtract(receive, emission, diff);
    if (st != JPL_OK) return st == JPL_ERROR_ARGUMENT ? JPL_ERROR_NUMERIC : st;
    elapsed = diff[0] + diff[1];
    if (!isfinite(elapsed)) return JPL_ERROR_NUMERIC;
    residual = elapsed - delay;
    if (!isfinite(residual)) return JPL_ERROR_NUMERIC;
    *delay_out = delay;
    *residual_out = residual;
    return JPL_OK;
}

/* Shared light-time solver behind jpl_get_state_lighttime and the private
 * deterministic test hook. Solves tau = range(target(tau emission),
 * observer(reception))/c for the delay tau with emission = receive - tau
 * as a two-part pair. Fast fixed-point iteration handles ordinary cases;
 * whenever an estimate crosses coverage or fails to settle, a sign-checked
 * bisection over emission epochs in [coverage_start, receive] recovers
 * valid near-start solutions (R11) and reports genuinely pre-coverage
 * roots as RANGE. State and delay are published only from one accepted
 * emission evaluation, so they always satisfy the residual within
 * JPL_LIGHTTIME_TOL_DAYS. Exhaustion of both budgets without an accepted
 * epoch is CONVERGENCE, never a silent estimate. Outputs stay unchanged
 * on every failure path. */
static jpl_status lighttime_solve(const jpl_ephemeris *e, const double jd[2],
                                  int target, int center, double state[6],
                                  double *delay, int fast_budget, int safe_budget) {
    double norm[2], cov_start[2], cov_end[2], emission[2];
    double observer_km[6], rel[6], candidate[6];
    double lt = 0.0, prev = 0.0, residual = 0.0;
    jpl_status st;
    int i, s;
    if (!e || !state || !jd) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd[0]) || !isfinite(jd[1])) return JPL_ERROR_ARGUMENT;
    if (fast_budget < 0 || safe_budget < 0) return JPL_ERROR_ARGUMENT;
    st = jpl_date_normalize(jd, norm);
    if (st != JPL_OK) return JPL_ERROR_ARGUMENT;
    /* Nutations/librations are angles with no light-time meaning; reject.
     * Bodies 0-13 only; JPL_EARTH_MOON_BARYCENTER (13) is the highest code.
     * Target 0 is consistently invalid here, matching jpl_get_state. */
    if (target <= JPL_SSB || target > JPL_EARTH_MOON_BARYCENTER || center < JPL_SSB || center > JPL_EARTH_MOON_BARYCENTER)
        return JPL_ERROR_ARGUMENT;
    /* Coverage validates before any state read so out-of-coverage queries
     * report RANGE with outputs untouched. */
    st = jpl_date_normalize(e->header.start_jd, cov_start);
    if (st != JPL_OK) return JPL_ERROR_FORMAT;
    st = jpl_date_normalize(e->header.end_jd, cov_end);
    if (st != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(norm, cov_start) < 0 || jpl_date_compare(norm, cov_end) > 0)
        return JPL_ERROR_RANGE;
    /* Observer barycentric state fixed at reception (raw file units). */
    if (center == JPL_SSB) memset(observer_km, 0, sizeof observer_km);
    else { st = barycentric_state(e, norm, center, observer_km); if (st != JPL_OK) return st; }
    /* Initial estimate: geometric delay at reception. A zero residual
     * here is an exact root (e.g. zero range); accept it regardless of
     * remaining budget. FORMAT/NUMERIC at the reception epoch propagates
     * immediately: the equation residual is undefined without a validated
     * target state, and falling through would invent a bracket. */
    st = lighttime_evaluate(e, target, norm, norm, observer_km, rel, &lt, &residual);
    if (st != JPL_OK) return st;
    if (fabs(residual) <= JPL_LIGHTTIME_TOL_DAYS) {
        memcpy(state, rel, sizeof rel);
        if (delay) *delay = lt;
        return JPL_OK;
    }
    /* Fast fixed-point iteration: emission = receive - lt via compensated
     * subtraction. A RANGE estimate crossed coverage, and unsettled
     * estimates after the budget, both fall through to the safeguarded
     * bracket rather than failing. FORMAT/NUMERIC at an iterate
     * propagates: the residual is undefined without a validated target
     * state there. Note the initial reception-epoch evaluation above
     * already covers zero-delay roots; the loop below must still run
     * before the bracket so ordinary interior roots converge without
     * bisection. The loop seeds prev from the validated initial delay
     * so an immediate settle is detected. */
    prev = lt;
    for (i = 0; i < fast_budget; ++i) {
        st = jpl_date_add_days(norm, -lt, emission);
        if (st != JPL_OK) break;
        st = lighttime_evaluate(e, target, norm, emission, observer_km, rel, &lt, &residual);
        if (st == JPL_ERROR_RANGE) break;
        if (st != JPL_OK) return st;
        if (fabs(lt - prev) <= JPL_LIGHTTIME_TOL_DAYS) {
            memcpy(state, rel, sizeof rel);
            if (delay) *delay = lt;
            return JPL_OK;
        }
        prev = lt;
    }
    /* Safeguarded fallback: bisect the emission epoch over the two-part
     * endpoints [coverage_start, receive]. At receive the residual is
     * -delay, hence nonpositive; at start a NEGATIVE residual can still
     * hold a valid root at the boundary itself (emission == coverage
     * start with residual -delay in (-1e-12, 0)), which the check below
     * accepts. Only a residual below -tolerance with no accepted epoch
     * means no in-coverage root exists under the subluminal-motion
     * contract, so the query is RANGE. Otherwise the sign bracket holds
     * a root. */
    {
        double lo[2], hi[2], mid[2];
        double lo_delay, hi_delay, mid_delay, lo_res, hi_res, mid_res;
        lo[0] = cov_start[0]; lo[1] = cov_start[1];
        hi[0] = norm[0]; hi[1] = norm[1];
        st = lighttime_evaluate(e, target, norm, lo, observer_km, candidate, &lo_delay, &lo_res);
        if (st != JPL_OK) return st;
        if (fabs(lo_res) <= JPL_LIGHTTIME_TOL_DAYS) {
            memcpy(state, candidate, sizeof candidate);
            if (delay) *delay = lo_delay;
            return JPL_OK;
        }
        if (lo_res < -JPL_LIGHTTIME_TOL_DAYS) return JPL_ERROR_RANGE;
        st = lighttime_evaluate(e, target, norm, hi, observer_km, candidate, &hi_delay, &hi_res);
        if (st != JPL_OK) return st;
        if (fabs(hi_res) <= JPL_LIGHTTIME_TOL_DAYS) {
            memcpy(state, candidate, sizeof candidate);
            if (delay) *delay = hi_delay;
            return JPL_OK;
        }
        /* hi_res = -delay <= 0 by construction (delay is non-negative),
         * so [lo, hi] brackets a sign change whenever lo_res > 0. */
        for (s = 0; s < safe_budget; ++s) {
            st = jpl_date_partition(lo, hi, 2, 1, mid);
            if (st != JPL_OK) return st;
            if (jpl_date_compare(mid, lo) == 0 || jpl_date_compare(mid, hi) == 0)
                return JPL_ERROR_CONVERGENCE;
            st = lighttime_evaluate(e, target, norm, mid, observer_km, candidate, &mid_delay, &mid_res);
            if (st != JPL_OK) return st;
            /* residual corresponds to the state evaluated at this candidate emission. */
            if (!isfinite(mid_res)) return JPL_ERROR_NUMERIC;
            if (fabs(mid_res) <= JPL_LIGHTTIME_TOL_DAYS) {
                memcpy(state, candidate, sizeof candidate);
                if (delay) *delay = mid_delay;
                return JPL_OK;
            }
            /* Update the sign bracket; never publish candidate here. */
            if (mid_res > 0.0) {
                lo[0] = mid[0]; lo[1] = mid[1];
                lo_delay = mid_delay; lo_res = mid_res;
            } else {
                hi[0] = mid[0]; hi[1] = mid[1];
                hi_delay = mid_delay; hi_res = mid_res;
            }
        }
        (void)hi_delay; (void)hi_res; (void)lo_delay; (void)lo_res;
        return JPL_ERROR_CONVERGENCE;
    }
}

#ifdef LIBJPL_TESTING
jpl_status jpl_test_lighttime_with_budget(const jpl_ephemeris *ephemeris,
    const double jd[2], int target, int center, double state[6], double *delay,
    int fast_iterations, int safeguarded_iterations) {
    return lighttime_solve(ephemeris, jd, target, center, state, delay,
                           fast_iterations, safeguarded_iterations);
}
#endif

jpl_status jpl_get_state_lighttime(const jpl_ephemeris *e, const double jd[2], int target, int center, double state[6], double *out_lt) {
    return lighttime_solve(e, jd, target, center, state, out_lt,
                           JPL_LIGHTTIME_FAST_MAX_ITER, JPL_LIGHTTIME_SAFE_MAX_ITER);
}

jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *e, const double jd_tdb[2], double *seconds, double *rate_per_day) {
    double state[6];
    double value, rate;
    jpl_status status;
    const jpl_pointer *tt;
    size_t span_end;
    if (!e || !seconds || !jd_tdb) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd_tdb[0]) || !isfinite(jd_tdb[1])) return JPL_ERROR_ARGUMENT;
    /* Valid checks precede the missing-dataset report: nonfinite/NULL
     * inputs above stay ARGUMENT, out-of-coverage below stays RANGE,
     * and only a valid in-coverage query for absent data is UNSUPPORTED. */
    {
        double norm[2], cov_start[2], cov_end[2];
        status = jpl_date_normalize(jd_tdb, norm);
        if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
        status = jpl_date_normalize(e->header.start_jd, cov_start);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        status = jpl_date_normalize(e->header.end_jd, cov_end);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        if (jpl_date_compare(norm, cov_start) < 0 || jpl_date_compare(norm, cov_end) > 0)
            return JPL_ERROR_RANGE;
    }
    if (!e->header.has_tt_tdb) return JPL_ERROR_UNSUPPORTED;
    /* Span end is the 1-based slot count (start-1)+coeff*sub, so equality
     * with the record width means the final subinterval ends exactly in
     * the last slot (e.g. DE440t start 1019 + 13*8 = 1122 in 1122
     * doubles). Only a span past the width is absent. */
    tt = &e->header.tt_tdb_pointer;
    if (tt_tdb_span_end(tt, &span_end) != 0 || span_end > e->header.record_double_count)
        return JPL_ERROR_UNSUPPORTED;
    /* One-component Chebyshev set: state[0] is TT-TDB in seconds and
     * state[3] its rate in seconds/day (span is in days, so the engine
     * derivative needs no unit conversion). Slots [1],[2],[4],[5] unused.
     * Validate before either optional/required output is written. */
    status = interpolate(e, jd_tdb, tt, 1, state);
    if (status != JPL_OK) return status;
    value = state[0];
    rate = state[3];
    if (!isfinite(value) || !isfinite(rate)) return JPL_ERROR_NUMERIC;
    *seconds = value;
    if (rate_per_day) *rate_per_day = rate;
    return JPL_OK;
}

jpl_status jpl_apply_aberration(const double state[6], const double observer_vel[3], double out[6]) {
    /* Signature-preserving wrapper: Sun distance unknown, so the <=0.4uas
     * solar term is omitted (see jpl_apply_aberration_full). */
    return jpl_apply_aberration_full(state, observer_vel, 0.0, out);
}

jpl_status jpl_apply_aberration_full(const double state[6], const double observer_vel[3],
                               double sun_distance_au, double out[6]) {
    /* Solar gravitational radius; ERFA ERFA_SRS, kept in jpl_private.h.
     * Published formula (cf. ERFA eraAb, Explanatory Supplement Expr. 7.40
     * + Klioner 2003 solar term): pnat = r/rn direction, beta = v/c,
     * bm1 = sqrt(1-|beta|^2), pdv = pnat.beta, w1 = 1+pdv/(1+bm1),
     * w2 = SRS/sun_distance, p = pnat*bm1 + w1*beta + w2*(beta-pdv*pnat),
     * distance-preserving rescale out = p*(r/|p|); velocity unchanged. */
    const double SRS = JPL_SRS_AU;
    double beta[3], pnat[3], p[3], local[6];
    double r, rn, beta2 = 0.0, bm1, pdv = 0.0, w1, w2;
    double local_state[6];
    int i;
    if (!state || !observer_vel || !out) return JPL_ERROR_ARGUMENT;
    for (i = 0; i < 6; ++i) {
        if (!isfinite(state[i])) return JPL_ERROR_ARGUMENT;
    }
    for (i = 0; i < 3; ++i) {
        if (!isfinite(observer_vel[i])) return JPL_ERROR_ARGUMENT;
        /* Velocity slot copy deferred: copy into local output only after
         * all validation below succeeds. */
    }
    if (!isfinite(sun_distance_au)) return JPL_ERROR_ARGUMENT;
    /* Observer-speed gate in AU/day before forming beta: a finite but
     * huge velocity is still at/above light speed (ARGUMENT), and this
     * avoids misclassifying the beta quotient overflow as NUMERIC.
     * Precedence: an unrepresentable observer norm reports NUMERIC;
     * a representable norm at/above light speed reports ARGUMENT. */
    {
        double vnorm = jpl_vec_norm3(observer_vel);
        if (!isfinite(vnorm)) return JPL_ERROR_NUMERIC;
        if (!(vnorm < JPL_C_AU_PER_DAY)) return JPL_ERROR_ARGUMENT;
    }
    for (i = 0; i < 3; ++i) {
        beta[i] = observer_vel[i] / JPL_C_AU_PER_DAY;
        if (!isfinite(beta[i])) return JPL_ERROR_NUMERIC;
        beta2 += beta[i] * beta[i];
        if (!isfinite(beta2)) return JPL_ERROR_NUMERIC;
    }
    /* Physically invalid standalone velocity: at or above light speed. */
    if (!(beta2 < 1.0)) return JPL_ERROR_ARGUMENT;
    { const double pos[3] = { state[0], state[1], state[2] }; r = jpl_vec_norm3(pos); }
    if (!isfinite(r)) return JPL_ERROR_NUMERIC;
    if (r == 0.0) {
        /* Validated finite zero position: pass through with unchanged
         * velocity. Copy via locals so aliased outputs stay exact. */
        memcpy(local, state, sizeof local);
        memcpy(out, local, sizeof local);
        return JPL_OK;
    }
    /* Scale-safe direction: normalize with the scaled components so r/rn
     * cannot overflow before a representable coordinate is multiplied.
     * With m = max|component| > 0, d = pos/m has |d| <= 1, and
     * pnat = d/|d| is exact up to roundoff; r is representable here. */
    {
        double m = 0.0, dl, d[3];
        for (i = 0; i < 3; ++i) {
            double a = fabs(state[i]);
            if (a > m) m = a;
        }
        for (i = 0; i < 3; ++i) d[i] = state[i] / m;
        dl = jpl_vec_norm3(d);
        if (!isfinite(dl) || dl == 0.0) return JPL_ERROR_NUMERIC;
        for (i = 0; i < 3; ++i) {
            pnat[i] = d[i] / dl;
            if (!isfinite(pnat[i])) return JPL_ERROR_NUMERIC;
        }
    }
    /* Full eraAb including the Klioner (2003) solar gravitational term;
     * finite nonpositive Sun distance explicitly omits the potential term
     * (wrapper behavior). Nonfinite Sun distance was rejected above. */
    bm1 = sqrt(1.0 - beta2);
    if (!isfinite(bm1)) return JPL_ERROR_NUMERIC;
    for (i = 0; i < 3; ++i) {
        pdv += pnat[i] * beta[i];
        if (!isfinite(pdv)) return JPL_ERROR_NUMERIC;
    }
    w1 = 1.0 + pdv / (1.0 + bm1);
    if (!isfinite(w1)) return JPL_ERROR_NUMERIC;
    /* SRS/sun_distance overflows only when the term is genuinely
     * unrepresentable; finite nonpositive distance omits it. */
    if (sun_distance_au > 0.0) {
        w2 = SRS / sun_distance_au;
        if (!isfinite(w2)) return JPL_ERROR_NUMERIC;
    } else {
        w2 = 0.0;
    }
    /* Second normalization is validated too: each accumulated square and
     * the final root must stay finite, else NUMERIC. */
    rn = 0.0;
    for (i = 0; i < 3; ++i) {
        double term = w2 * (beta[i] - pdv * pnat[i]);
        if (!isfinite(term)) return JPL_ERROR_NUMERIC;
        p[i] = pnat[i] * bm1 + w1 * beta[i] + term;
        if (!isfinite(p[i])) return JPL_ERROR_NUMERIC;
        rn += p[i] * p[i];
        if (!isfinite(rn)) return JPL_ERROR_NUMERIC;
    }
    rn = sqrt(rn);
    if (!isfinite(rn) || !(rn > 0.0)) return JPL_ERROR_NUMERIC;
    /* Scale-aware restoration: (p/rn)*r cannot overflow before a
     * representable coordinate is multiplied. The r/rn quotient form is
     * intentionally unused: dividing each of p by rn first keeps the
     * intermediate direction bounded. */
    for (i = 0; i < 3; ++i) {
        local_state[i] = (p[i] / rn) * r;
        if (!isfinite(local_state[i])) return JPL_ERROR_NUMERIC;
    }
    /* Velocity passed through unchanged, copied into local output only
     * after every check above succeeded. */
    local_state[3] = state[3]; local_state[4] = state[4]; local_state[5] = state[5];
    memcpy(out, local_state, sizeof local_state);
    return JPL_OK;
}
