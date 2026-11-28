/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"

#include <limits.h>
#include <string.h>

static void set_pointer(jpl_header *h, int start, int coeffs, int sub) {
    h->tt_tdb_pointer.start_index = start;
    h->tt_tdb_pointer.coefficients_per_component = coeffs;
    h->tt_tdb_pointer.subintervals = sub;
}

int main(void) {
    jpl_header h;
    jpl_tt_tdb_table table;
    size_t end = 0;

    /* Absent dataset. */
    memset(&h, 0, sizeof h);
    h.record_double_count = 100;
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 0);

    /* Valid span, one slot before the record end. */
    h.has_tt_tdb = 1;
    set_pointer(&h, 90, 2, 5);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 1);
    JPL_TEST_CHECK(table.span_end == 99);
    JPL_TEST_CHECK(table.pointer.start_index == 90);

    /* Span ending exactly at the record width is usable. */
    set_pointer(&h, 91, 2, 5);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 1);
    JPL_TEST_CHECK(table.span_end == 100);

    /* Span past the width is absent. */
    set_pointer(&h, 92, 2, 5);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 0);

    /* Non-positive fields and overflow are absent. */
    set_pointer(&h, 0, 2, 5);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 0);
    set_pointer(&h, 1, 0, 5);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 0);
    set_pointer(&h, INT_MAX, 2, 2);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, &table) == JPL_OK);
    JPL_TEST_CHECK(table.present == 0);

    /* NULL arguments. */
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(NULL, &table) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_tt_tdb_table_build(&h, NULL) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_tt_tdb_span_end(NULL, &end) == -1);
    JPL_TEST_CHECK(jpl_tt_tdb_span_end(&h.tt_tdb_pointer, NULL) == -1);
    return 0;
}
