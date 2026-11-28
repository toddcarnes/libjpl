/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"

#include <limits.h>
#include <string.h>

/* Exclusive upper record-double index for a TT-TDB coefficient span, or -1
 * when the pointer fields are absent or overflow. */
int jpl_tt_tdb_span_end(const jpl_pointer *pointer, size_t *out) {
    size_t product, end;
    if (!pointer || !out) return -1;
    if (pointer->start_index <= 0 || pointer->coefficients_per_component <= 0 ||
        pointer->subintervals <= 0) return -1;
    if ((size_t)pointer->subintervals > SIZE_MAX / (size_t)pointer->coefficients_per_component)
        return -1;
    product = (size_t)pointer->coefficients_per_component * (size_t)pointer->subintervals;
    if (product > SIZE_MAX - ((size_t)pointer->start_index - 1)) return -1;
    end = ((size_t)pointer->start_index - 1) + product;
    *out = end;
    return 0;
}

jpl_status jpl_tt_tdb_table_build(const jpl_header *header,
                                  jpl_tt_tdb_table *out) {
    size_t span_end;
    if (!header || !out) return JPL_ERROR_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!header->has_tt_tdb) return JPL_OK;
    if (jpl_tt_tdb_span_end(&header->tt_tdb_pointer, &span_end) != 0)
        return JPL_OK;
    if (span_end > header->record_double_count) return JPL_OK;
    out->present = 1;
    out->pointer = header->tt_tdb_pointer;
    out->span_end = span_end;
    return JPL_OK;
}
