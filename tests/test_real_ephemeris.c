/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    jpl_ephemeris *ephemeris;
    const jpl_header *header;
    double *constants;
    double *record;
    double state[6];

    jpl_status status;
    if (argc != 2) return 2;
    status = jpl_open(argv[1], &ephemeris);
    if (status != JPL_OK) {
        fprintf(stderr, "Could not open %s: %s\n", argv[1], jpl_status_string(status));
        return 1;
    }
    header = jpl_get_header(ephemeris);
    if (header == NULL || header->ephemeris_number <= 0 ||
        header->start_jd[0] >= header->end_jd[0] || header->constant_count <= 0 ||
        header->record_double_count <= 2 || jpl_record_count(ephemeris) == 0) {
        fputs("Unexpected real-ephemeris header values.\n", stderr);
        jpl_close(ephemeris);
        return 1;
    }

    constants = malloc((size_t)header->constant_count * sizeof *constants);
    record = malloc(header->record_double_count * sizeof *record);
    if (constants == NULL || record == NULL ||
        jpl_read_constants(ephemeris, constants, (size_t)header->constant_count) != JPL_OK ||
        jpl_read_record(ephemeris, 0, record, header->record_double_count) != JPL_OK ||
        jpl_date_compare((const double[2]){record[0], 0.0}, header->start_jd) < 0 ||
        jpl_date_compare((const double[2]){record[1], 0.0}, header->end_jd) > 0 ||
        record[1] <= record[0]) {
        fputs("Could not read valid ephemeris data.\n", stderr);
        free(record);
        free(constants);
        jpl_close(ephemeris);
        return 1;
    }
    /* Query 100 days after coverage start through the compensated helper so
     * no scalar full-JD adapter is introduced for the migration. */
    {
        double query[2];
        status = jpl_date_add_days(header->start_jd, 100.0, query);
        if (status != JPL_OK ||
            jpl_get_state(ephemeris, query, JPL_EARTH, JPL_SUN, state) != JPL_OK ||
            !isfinite(state[0]) || !isfinite(state[1]) || !isfinite(state[2]) ||
            !isfinite(state[3]) || !isfinite(state[4]) || !isfinite(state[5])) {
            fputs("Could not interpolate an ephemeris state vector.\n", stderr);
            free(record);
            free(constants);
            jpl_close(ephemeris);
            return 1;
        }
        (void)status;
    }

    free(record);
    free(constants);
    jpl_close(ephemeris);
    puts("real ephemeris integration test passed");
    return 0;
}
