/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define STORE_FSEEK _fseeki64
typedef __int64 store_file_offset;
#else
#define STORE_FSEEK fseeko
typedef off_t store_file_offset;
#endif

static int store_seek(FILE *file, size_t offset) {
#if SIZE_MAX > INT64_MAX
    if ((uintmax_t)offset > (uintmax_t)INT64_MAX) return -1;
#endif
    return STORE_FSEEK(file, (store_file_offset)offset, SEEK_SET);
}

static jpl_byte_order store_native_order(void) {
    const uint16_t probe = 1;
    return *(const unsigned char *)&probe ? JPL_BYTE_ORDER_LITTLE_ENDIAN
                                          : JPL_BYTE_ORDER_BIG_ENDIAN;
}

static double store_decode_f64(const unsigned char *p, jpl_byte_order order) {
    uint64_t bits;
    double value;
    memcpy(&bits, p, sizeof bits);
    if (order != store_native_order()) {
#if defined(_MSC_VER)
        bits = _byteswap_uint64(bits);
#elif defined(__GNUC__) || defined(__clang__)
        bits = __builtin_bswap64(bits);
#else
        bits = ((bits & UINT64_C(0x00000000000000FF)) << 56) |
               ((bits & UINT64_C(0x000000000000FF00)) << 40) |
               ((bits & UINT64_C(0x0000000000FF0000)) << 24) |
               ((bits & UINT64_C(0x00000000FF000000)) << 8) |
               ((bits & UINT64_C(0x000000FF00000000)) >> 8) |
               ((bits & UINT64_C(0x0000FF0000000000)) >> 24) |
               ((bits & UINT64_C(0x00FF000000000000)) >> 40) |
               ((bits & UINT64_C(0xFF00000000000000)) >> 56);
#endif
    }
    memcpy(&value, &bits, sizeof value);
    return value;
}

jpl_status jpl_mmap_read_bytes(void *context, size_t offset,
                               unsigned char *out, size_t count) {
    const jpl_mmap_source *source = (const jpl_mmap_source *)context;
    if (!source || !source->base || !out) return JPL_ERROR_ARGUMENT;
    if (offset > source->size || count > source->size - offset) return JPL_ERROR_IO;
    memcpy(out, source->base + offset, count);
    return JPL_OK;
}

jpl_status jpl_file_read_bytes(void *context, size_t offset,
                               unsigned char *out, size_t count) {
    const jpl_file_source *source = (const jpl_file_source *)context;
    if (!source || !source->file || !out) return JPL_ERROR_ARGUMENT;
    if (offset > source->size || count > source->size - offset) return JPL_ERROR_IO;
    if (store_seek(source->file, offset) != 0) return JPL_ERROR_IO;
    return fread(out, 1, count, source->file) == count ? JPL_OK : JPL_ERROR_IO;
}

jpl_status jpl_store_init_file(jpl_record_store *store, FILE *file,
                               size_t size, jpl_byte_order byte_order) {
    if (!store || !file) return JPL_ERROR_ARGUMENT;
    memset(store, 0, sizeof *store);
    store->file.file = file;
    store->file.size = size;
    store->source.context = &store->file;
    store->source.size = size;
    store->source.read = jpl_file_read_bytes;
    store->is_mmap = 0;
    store->byte_order = byte_order;
    return JPL_OK;
}

jpl_status jpl_store_init_mmap(jpl_record_store *store,
                               const unsigned char *base, size_t size,
                               jpl_byte_order byte_order) {
    if (!store || !base) return JPL_ERROR_ARGUMENT;
    memset(store, 0, sizeof *store);
    store->mmap.base = base;
    store->mmap.size = size;
    store->source.context = &store->mmap;
    store->source.size = size;
    store->source.read = jpl_mmap_read_bytes;
    store->is_mmap = 1;
    store->byte_order = byte_order;
    return JPL_OK;
}

jpl_status jpl_store_read_raw(const jpl_record_store *store, size_t offset,
                              void *out, size_t count) {
    if (!store || !out || !store->source.read) return JPL_ERROR_ARGUMENT;
    return store->source.read(store->source.context, offset,
                              (unsigned char *)out, count);
}

jpl_status jpl_store_read_doubles(const jpl_record_store *store, size_t offset,
                                  double *values, size_t count) {
    unsigned char *bytes;
    size_t i, bytes_count;
    jpl_status status;
    if (!store || !values || !store->source.read) return JPL_ERROR_ARGUMENT;
    if (count == 0) return JPL_OK;
    if (count > SIZE_MAX / 8) return JPL_ERROR_IO;
    bytes_count = count * 8;
    if (store->byte_order == store_native_order()) {
        return store->source.read(store->source.context, offset,
                                  (unsigned char *)values, bytes_count);
    }
    bytes = (unsigned char *)malloc(bytes_count);
    if (!bytes) return JPL_ERROR_MEMORY;
    status = store->source.read(store->source.context, offset, bytes, bytes_count);
    if (status != JPL_OK) { free(bytes); return status; }
    for (i = 0; i < count; ++i) values[i] = store_decode_f64(bytes + i * 8, store->byte_order);
    free(bytes);
    return JPL_OK;
}

/* Ensure the FILE-path cache owns a record buffer. */
static jpl_status store_cache_reserve(jpl_record_store *store, size_t record_doubles) {
    if (!store->cache.record) {
        store->cache.record = (double *)malloc(record_doubles * sizeof(double));
        if (!store->cache.record) return JPL_ERROR_MEMORY;
        store->cache.valid = 0;
    }
    return JPL_OK;
}

jpl_status jpl_store_record(jpl_record_store *store, size_t record_doubles,
                            size_t index, jpl_record_view *out) {
    size_t record_bytes, offset;
    jpl_status status;
    if (!store || !out || record_doubles == 0) return JPL_ERROR_ARGUMENT;
    if (record_doubles > SIZE_MAX / 8) return JPL_ERROR_MEMORY;
    record_bytes = record_doubles * 8;
    /* Data record index 0 follows the header and constants records. */
    {
        size_t max_index = SIZE_MAX / record_bytes;
        if (max_index < 2 || index > max_index - 2) return JPL_ERROR_IO;
    }
    offset = (index + 2) * record_bytes;
    if (offset > store->source.size || record_bytes > store->source.size - offset)
        return JPL_ERROR_IO;

    if (store->byte_order == store_native_order()) {
        if (store->is_mmap) {
            out->values = (const double *)(const void *)(store->mmap.base + offset);
            out->owned = NULL;
            return JPL_OK;
        }
        status = store_cache_reserve(store, record_doubles);
        if (status != JPL_OK) return status;
        if (!(store->cache.valid && store->cache.index == index)) {
            status = store->source.read(store->source.context, offset,
                                        (unsigned char *)store->cache.record, record_bytes);
            if (status != JPL_OK) return status;
            store->cache.index = index;
            store->cache.valid = 1;
        }
        out->values = store->cache.record;
        out->owned = NULL;
        return JPL_OK;
    }

    if (store->is_mmap) {
        double *decoded = (double *)malloc(record_doubles * sizeof(double));
        unsigned char *bytes = (unsigned char *)malloc(record_bytes);
        size_t i;
        if (!decoded || !bytes) { free(decoded); free(bytes); return JPL_ERROR_MEMORY; }
        status = store->source.read(store->source.context, offset, bytes, record_bytes);
        if (status != JPL_OK) { free(decoded); free(bytes); return status; }
        for (i = 0; i < record_doubles; ++i)
            decoded[i] = store_decode_f64(bytes + i * 8, store->byte_order);
        free(bytes);
        out->values = decoded;
        out->owned = decoded;
        return JPL_OK;
    }

    status = store_cache_reserve(store, record_doubles);
    if (status != JPL_OK) return status;
    if (!(store->cache.valid && store->cache.index == index)) {
        status = jpl_store_read_doubles(store, offset, store->cache.record, record_doubles);
        if (status != JPL_OK) return status;
        store->cache.index = index;
        store->cache.valid = 1;
    }
    out->values = store->cache.record;
    out->owned = NULL;
    return JPL_OK;
}

void jpl_record_view_release(jpl_record_view *view) {
    if (!view) return;
    free(view->owned);
    view->owned = NULL;
    view->values = NULL;
}
