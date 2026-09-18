#include "jpl.h"

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
        header->start_jd >= header->end_jd || header->constant_count <= 0 ||
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
        record[0] < header->start_jd || record[1] > header->end_jd || record[1] <= record[0]) {
        fputs("Could not read valid ephemeris data.\n", stderr);
        free(record);
        free(constants);
        jpl_close(ephemeris);
        return 1;
    }
    if (jpl_get_state(ephemeris, header->start_jd + 100.0, JPL_EARTH, JPL_SUN, state) != JPL_OK ||
        !isfinite(state[0]) || !isfinite(state[1]) || !isfinite(state[2]) ||
        !isfinite(state[3]) || !isfinite(state[4]) || !isfinite(state[5])) {
        fputs("Could not interpolate an ephemeris state vector.\n", stderr);
        free(record);
        free(constants);
        jpl_close(ephemeris);
        return 1;
    }

    free(record);
    free(constants);
    jpl_close(ephemeris);
    puts("real ephemeris integration test passed");
    return 0;
}
