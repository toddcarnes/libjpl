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
#endif

enum { HEADER_SIZE = 2856 };

struct jpl_ephemeris {
    FILE *file;
    jpl_header header;
    size_t file_size;
    size_t record_count;
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

static char *copy_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
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

/* Record-layout derivation shared by jpl_open and the mmap open paths
 * (header + constants + N data records, with a one-record legacy trailing
 * allowance); updates header->record_double_count. */
static jpl_status derive_mmap_layout(jpl_header *header, size_t file_size, size_t *out_record_count) {
    double span_records = (header->end_jd - header->start_jd) / header->step_days;
    size_t expected_records, total_records = 0, record_bytes, extra_records;
    if (span_records < 1.0 || span_records > (double)(SIZE_MAX - 2)) return JPL_ERROR_FORMAT;
    expected_records = (size_t)(span_records + 0.5);
    if (fabs(span_records - (double)expected_records) > 1e-7 || expected_records > SIZE_MAX - 2) return JPL_ERROR_FORMAT;
    record_bytes = 0;
    for (extra_records = 2; extra_records <= 3; ++extra_records) {
        if (expected_records > SIZE_MAX - extra_records) break;
        total_records = expected_records + extra_records;
        if (file_size % total_records != 0) continue;
        record_bytes = file_size / total_records;
        if (record_bytes % 8 == 0 && record_bytes / 8 >= header->record_double_count &&
            (size_t)header->constant_count <= record_bytes / 8) break;
        record_bytes = 0;
    }
    if (record_bytes == 0) return JPL_ERROR_FORMAT;
    header->record_double_count = record_bytes / 8;
    *out_record_count = total_records - 2;
    return JPL_OK;
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
    if (offset > (size_t)INT64_MAX) return -1;
    return JPL_FSEEK(file, (jpl_file_offset)offset, SEEK_SET);
}

static int checked_record_offset(size_t index, size_t record_doubles, size_t *out) {
    size_t a, b;
    if (record_doubles == 0 || record_doubles > SIZE_MAX / 8) return -1;
    b = record_doubles * 8;
    if (index > (SIZE_MAX / b) - 2) return -1;
    a = (index + 2) * b;
    *out = a;
    return 0;
}

static void copy_text(char *destination, const unsigned char *source, size_t length) {
    memcpy(destination, source, length);
    while (length > 0 && (destination[length - 1] == ' ' || destination[length - 1] == '\0')) --length;
    destination[length] = '\0';
}

static jpl_status parse_header(jpl_header *header, const unsigned char *bytes, jpl_byte_order order) {
    size_t i, maximum = 2;
    const unsigned char *p = bytes;
    memset(header, 0, sizeof *header);
    for (i = 0; i < 3; ++i) copy_text(header->titles[i], p + i * 84, 84);
    p += JPL_HDR_TITLES_LEN;
    for (i = 0; i < JPL_MAX_CONSTANT_NAMES; ++i) copy_text(header->constant_names[i], p + i * 6, 6);
    p += JPL_HDR_TEXT_LEN;
    header->start_jd = read_f64(p, order); p += 8;
    header->end_jd = read_f64(p, order); p += 8;
    header->step_days = read_f64(p, order); p += 8;
    header->constant_count = 0;
    { uint32_t u_nconst = read_u32(p, order); p += 4; if (u_nconst > INT_MAX) return JPL_ERROR_FORMAT; header->constant_count = (int)u_nconst; }
    header->astronomical_unit_km = read_f64(p, order); p += 8;
    header->earth_moon_mass_ratio = read_f64(p, order); p += 8;
    if (!plausible_date(header->start_jd) || !plausible_date(header->end_jd) ||
        header->end_jd <= header->start_jd || header->step_days <= 0.0) return JPL_ERROR_FORMAT;
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
    { uint32_t u_denum = read_u32(p, order); p += 4; if (u_denum > INT_MAX) return JPL_ERROR_FORMAT; header->ephemeris_number = (int)u_denum; }
    { uint32_t u_lstart = read_u32(p, order); p += 4; if (u_lstart > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.start_index = (int)u_lstart; }
    { uint32_t u_lcoeff = read_u32(p, order); p += 4; if (u_lcoeff > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.coefficients_per_component = (int)u_lcoeff; }
    { uint32_t u_lsub = read_u32(p, order); if (u_lsub > INT_MAX) return JPL_ERROR_FORMAT; header->libration_pointer.subintervals = (int)u_lsub; }
    if (header->libration_pointer.start_index > 0 && header->libration_pointer.coefficients_per_component > 0 && header->libration_pointer.subintervals > 0) {
        size_t lcap = SIZE_MAX / 3u / (size_t)header->libration_pointer.subintervals;
        if (lcap == 0 || (size_t)header->libration_pointer.coefficients_per_component >= lcap) return JPL_ERROR_FORMAT;
        size_t lproduct = 3u * (size_t)header->libration_pointer.coefficients_per_component * (size_t)header->libration_pointer.subintervals;
        size_t end;
        if (lproduct > SIZE_MAX - ((size_t)header->libration_pointer.start_index - 1)) return JPL_ERROR_FORMAT;
        end = ((size_t)header->libration_pointer.start_index - 1) + lproduct;
        if (end > maximum) maximum = end;
    }
    header->record_double_count = maximum;
    header->byte_order = order;
    return JPL_OK;
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

const char *jpl_status_string(jpl_status status) { static const char *const text[] = { "success", "invalid argument", "I/O error", "invalid JPL DE format", "out of range", "out of memory" }; return status >= JPL_OK && status <= JPL_ERROR_MEMORY ? text[status] : "unknown error"; }

jpl_status jpl_open(const char *path, jpl_ephemeris **out) {
    unsigned char bytes[HEADER_SIZE]; jpl_ephemeris *e; jpl_status status;
    if (!path || !out) return JPL_ERROR_ARGUMENT; *out = NULL;
    e = calloc(1, sizeof *e); if (!e) return JPL_ERROR_MEMORY;
    e->refcount = malloc(sizeof *e->refcount); if (!e->refcount) { free(e); return JPL_ERROR_MEMORY; }
    refcount_init(e->refcount);
    e->is_mmap = 0; e->mmap_base = NULL; e->mmap_size = 0;
    e->path = copy_string(path); if (!e->path) { free(e->refcount); free(e); return JPL_ERROR_MEMORY; }
    e->file = fopen(path, "rb"); if (!e->file) { jpl_close(e); return JPL_ERROR_IO; }
    if (fread(bytes, 1, sizeof bytes, e->file) != sizeof bytes || JPL_FSEEK(e->file, 0, SEEK_END) != 0) { jpl_close(e); return JPL_ERROR_IO; }
    { jpl_file_offset end = JPL_FTELL(e->file); if (end < 0 || (uintmax_t)end > SIZE_MAX) { jpl_close(e); return JPL_ERROR_IO; } e->file_size = (size_t)end; }
    status = parse_header(&e->header, bytes, JPL_BYTE_ORDER_LITTLE_ENDIAN);
    if (status != JPL_OK) status = parse_header(&e->header, bytes, JPL_BYTE_ORDER_BIG_ENDIAN);
    if (status != JPL_OK || e->header.record_double_count == 0 || e->header.record_double_count > SIZE_MAX / 8) { jpl_close(e); return JPL_ERROR_FORMAT; }
    /* Most files have a header, constants record, and exactly one record
     * per header interval. Some legacy DE files retain one trailing data
     * record, so look for a compatible total within that small allowance. */
    status = derive_mmap_layout(&e->header, e->file_size, &e->record_count);
    if (status != JPL_OK) { jpl_close(e); return JPL_ERROR_FORMAT; }
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
        if (!GetFileSizeEx(hFile, &size) || size.QuadPart < (LONGLONG)HEADER_SIZE ||
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
        status = parse_header(&header, base, JPL_BYTE_ORDER_LITTLE_ENDIAN);
        if (status != JPL_OK) status = parse_header(&header, base, JPL_BYTE_ORDER_BIG_ENDIAN);
        if (status == JPL_OK && (header.record_double_count == 0 || header.record_double_count > SIZE_MAX / 8))
            status = JPL_ERROR_FORMAT;
        if (status == JPL_OK) status = derive_mmap_layout(&header, file_size, &record_count);
        if (status != JPL_OK) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return status; }
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
        if (fstat(fd, &st) != 0 || st.st_size < (off_t)HEADER_SIZE || (uintmax_t)st.st_size > (uintmax_t)SIZE_MAX) {
            close(fd);
            return JPL_ERROR_IO;
        }
        file_size = (size_t)st.st_size;
        base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (base == MAP_FAILED) return JPL_ERROR_IO;
        /* Re-check size after mapping: the file must not change under us
         * (truncation after mapping faults on access; growth is ignored). */
        {
            struct stat rest;
            int rfd = open(path, O_RDONLY);
            if (rfd >= 0) {
                if (fstat(rfd, &rest) != 0 || rest.st_size != st.st_size) {
                    close(rfd); munmap(base, file_size); return JPL_ERROR_IO;
                }
                close(rfd);
            }
        }
        status = parse_header(&header, base, JPL_BYTE_ORDER_LITTLE_ENDIAN);
        if (status != JPL_OK) status = parse_header(&header, base, JPL_BYTE_ORDER_BIG_ENDIAN);
        if (status == JPL_OK && (header.record_double_count == 0 || header.record_double_count > SIZE_MAX / 8))
            status = JPL_ERROR_FORMAT;
        if (status == JPL_OK) status = derive_mmap_layout(&header, file_size, &record_count);
        if (status != JPL_OK) { munmap(base, file_size); return status; }
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
    if (!ephem || !out_clone) return JPL_ERROR_ARGUMENT;
    *out_clone = NULL;
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
        c->file = fopen(ephem->path, "rb");
        if (!c->file) { free(c->path); free(c->refcount); free(c); return JPL_ERROR_IO; }
        /* Cold start: calloc zeroed cache_valid, so this clone never reads
         * the source handle's cached record. */
    }
    *out_clone = c;
    return JPL_OK;
}
const jpl_header *jpl_get_header(const jpl_ephemeris *e) { return e ? &e->header : NULL; }
size_t jpl_record_count(const jpl_ephemeris *e) { return e ? e->record_count : 0; }
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

static jpl_status interpolate(const jpl_ephemeris *e, double jd, const jpl_pointer *pointer,
                              size_t components, double state[6]) {
    double *record, record_start, record_end, substep, normalized;
    size_t record_index, subinterval, component, coefficient;
    int cached, use_cache;
    jpl_status status;
    if (pointer->start_index <= 0 || pointer->coefficients_per_component <= 0 ||
        pointer->subintervals <= 0 || jd < e->header.start_jd || jd > e->header.end_jd) return JPL_ERROR_RANGE;
    record_index = (size_t)((jd - e->header.start_jd) / e->header.step_days);
    if (record_index >= e->record_count) record_index = e->record_count - 1;
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
    record_start = record[0]; record_end = record[1];
    if (jd < record_start || jd > record_end || record_end <= record_start) { free(record); return JPL_ERROR_FORMAT; }
    substep = (record_end - record_start) / pointer->subintervals;
    subinterval = (size_t)((jd - record_start) / substep);
    if (subinterval >= (size_t)pointer->subintervals) subinterval = (size_t)pointer->subintervals - 1;
    normalized = 2.0 * (jd - (record_start + subinterval * substep)) / substep - 1.0;
    for (component = 0; component < components; ++component) {
        size_t start = (size_t)pointer->start_index - 1 +
            (subinterval * components + component) * (size_t)pointer->coefficients_per_component;
        double sum, derivative = 0.0;
        if (start + (size_t)pointer->coefficients_per_component > e->header.record_double_count) { free(record); return JPL_ERROR_FORMAT; }
        sum = record[start];
        if (pointer->coefficients_per_component > 1) sum += record[start + 1] * normalized;
        /* Re-evaluate derivative with the standard Chebyshev recurrence. */
        if (pointer->coefficients_per_component > 1) {
            double t0 = 1.0, t1 = normalized, d0 = 0.0, d1 = 1.0;
            derivative = record[start + 1];
            for (coefficient = 2; coefficient < (size_t)pointer->coefficients_per_component; ++coefficient) {
                double t2 = 2.0 * normalized * t1 - t0;
                double d2 = 2.0 * t1 + 2.0 * normalized * d1 - d0;
                sum += record[start + coefficient] * t2;
                derivative += record[start + coefficient] * d2;
                t0 = t1; t1 = t2; d0 = d1; d1 = d2;
            }
        }
        state[component] = sum;
        state[component + 3] = derivative * 2.0 / substep;
    }
    /* Nutations interpolate 2 components; zero the unused third slot. */
    for (; component < 3; ++component) { state[component] = 0.0; state[component + 3] = 0.0; }
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

static jpl_status barycentric_state(const jpl_ephemeris *e, double jd, int target, double state[6]) {
    double moon[6]; size_t i; jpl_status status;
    if (target == JPL_SOLAR_SYSTEM_BARYCENTER) { memset(state, 0, 6 * sizeof *state); return JPL_OK; }
    if (target >= JPL_MERCURY && target <= JPL_SUN && target != JPL_MOON)
        return interpolate(e, jd, &e->header.pointers[target - 1], 3, state);
    if (target != JPL_EARTH && target != JPL_MOON) return JPL_ERROR_ARGUMENT;
    status = interpolate(e, jd, &e->header.pointers[JPL_EARTH_MOON_BARYCENTER - 1], 3, state);
    if (status != JPL_OK) return status;
    status = interpolate(e, jd, &e->header.pointers[JPL_MOON - 1], 3, moon);
    if (status != JPL_OK) return status;
    for (i = 0; i < 6; ++i) state[i] -= moon[i] / (1.0 + e->header.earth_moon_mass_ratio);
    if (target == JPL_MOON) for (i = 0; i < 6; ++i) state[i] += moon[i];
    return JPL_OK;
}

jpl_status jpl_get_state(const jpl_ephemeris *e, double jd, int target, int center, double state[6]) {
    double target_state[6], center_state[6]; size_t i; jpl_status status;
    if (!e || !state) return JPL_ERROR_ARGUMENT;
    if (target == JPL_NUTATIONS || target == JPL_LIBRATIONS) {
        if (center != 0) return JPL_ERROR_ARGUMENT;
        return interpolate(e, jd, target == JPL_NUTATIONS ? &e->header.pointers[JPL_TARGET_COUNT - 1] : &e->header.libration_pointer,
                           target == JPL_NUTATIONS ? 2 : 3, state);
    }
    status = barycentric_state(e, jd, target, target_state);
    if (status != JPL_OK) return status;
    if (center == 0) memset(center_state, 0, sizeof center_state);
    else { status = barycentric_state(e, jd, center, center_state); if (status != JPL_OK) return status; }
    for (i = 0; i < 6; ++i) state[i] = (target_state[i] - center_state[i]) / e->header.astronomical_unit_km;
    return JPL_OK;
}

static double range_days(const double state[6]) {
    const double pos[3] = { state[0], state[1], state[2] };
    return jpl_vec_norm3(pos) / JPL_C_AU_PER_DAY;
}

jpl_status jpl_get_state_lighttime(const jpl_ephemeris *e, double jd, int target, int center, double state[6], double *out_lt) {
    double rel[6], tgt[6], ctr[6], lt = 0.0, prev = 0.0;
    jpl_status st;
    int i;
    size_t k;
    if (!e || !state) return JPL_ERROR_ARGUMENT;
    /* Nutations/librations are angles with no light-time meaning; reject. */
    if (target < 0 || target > JPL_EARTH || center < 0 || center > JPL_EARTH) return JPL_ERROR_ARGUMENT;
    /* Center evaluated once at reception jd; only the target leg iterates. */
    if (center == 0) memset(ctr, 0, sizeof ctr);
    else { st = barycentric_state(e, jd, center, ctr); if (st != JPL_OK) return st; }
    st = jpl_get_state(e, jd, target, center, rel);
    if (st != JPL_OK) return st;
    lt = range_days(rel);
    for (i = 0; i < JPL_LIGHTTIME_MAX_ITER; ++i) {
        prev = lt;
        st = barycentric_state(e, jd - lt, target, tgt);
        if (st != JPL_OK) return st;
        for (k = 0; k < 6; ++k) rel[k] = (tgt[k] - ctr[k]) / e->header.astronomical_unit_km;
        lt = range_days(rel);
        if (fabs(lt - prev) < JPL_LIGHTTIME_TOL_DAYS) break;
    }
    if (fabs(lt - prev) >= JPL_LIGHTTIME_TOL_DAYS) return JPL_ERROR_RANGE;
    memcpy(state, rel, sizeof rel);
    if (out_lt) *out_lt = lt;
    return JPL_OK;
}

void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out[6]) {
    /* Signature-preserving wrapper: Sun distance unknown, so the <=0.4uas
     * solar term is omitted (see jpl_apply_aberration_full). */
    jpl_apply_aberration_full(state, observer_vel, 0.0, out);
}

void jpl_apply_aberration_full(const double state[6], const double observer_vel[3],
                               double sun_distance_au, double out[6]) {
    /* Solar gravitational radius; ERFA ERFA_SRS, kept in jpl_private.h. */
    const double SRS = JPL_SRS_AU;
    double beta[3], pnat[3], p[3];
    double r, rn, beta2 = 0.0, bm1, pdv = 0.0, w1, w2;
    int i;
    if (!state || !observer_vel || !out) return;
    { const double pos[3] = { state[0], state[1], state[2] }; r = jpl_vec_norm3(pos); }
    if (!(r > 0.0)) { memcpy(out, state, 6 * sizeof *out); return; }
    for (i = 0; i < 3; ++i) {
        pnat[i] = state[i] / r;
        beta[i] = observer_vel[i] / JPL_C_AU_PER_DAY;
        beta2 += beta[i]*beta[i];
    }
    if (!(beta2 < 1.0)) { memcpy(out, state, 6 * sizeof *out); return; }
    /* Full eraAb including the Klioner (2003) solar gravitational term;
     * non-positive/non-finite Sun distance falls back to wrapper behavior. */
    bm1 = sqrt(1.0 - beta2);
    for (i = 0; i < 3; ++i) pdv += pnat[i]*beta[i];
    w1 = 1.0 + pdv/(1.0 + bm1);
    w2 = (sun_distance_au > 0.0 && isfinite(sun_distance_au)) ? SRS/sun_distance_au : 0.0;
    rn = 0.0;
    for (i = 0; i < 3; ++i) { p[i] = pnat[i]*bm1 + w1*beta[i] + w2*(beta[i] - pdv*pnat[i]); rn += p[i]*p[i]; }
    rn = sqrt(rn);
    if (!(rn > 0.0)) { memcpy(out, state, 6 * sizeof *out); return; }
    for (i = 0; i < 3; ++i) out[i] = p[i] * (r / rn);
    /* Velocity passed through unchanged. */
    out[3]=state[3]; out[4]=state[4]; out[5]=state[5];
}
