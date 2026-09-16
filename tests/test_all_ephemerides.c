#include "jpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    static const char *const filenames[] = {
        "DE102.eph", "DE200.eph", "DE202.eph", "DE403.eph", "DE404.eph",
        "DE405.eph", "DE406.eph", "DE406e.eph", "DE410.eph", "DE413.eph",
        "DE414.eph", "DE418.eph", "DE421.eph", "DE422.eph", "DE423.eph",
        "DE424.eph", "DE430.eph", "DE430t.eph", "DE431.eph", "DE432.eph",
        "DE432t.eph", "DE433.eph", "DE434.eph", "DE435.eph", "DE436.eph",
        "DE436t.eph", "DE438.eph", "DE438t.eph", "DE440.eph", "DE440t.eph",
        "DE441.eph"
    };
    char path[1024];
    size_t i;
    if (argc != 2) return 2;
    for (i = 0; i < sizeof filenames / sizeof filenames[0]; ++i) {
        jpl_ephemeris *ephemeris = NULL;
        const jpl_header *header;
        jpl_status status;
        double *record;
        if (snprintf(path, sizeof path, "%s/%s", argv[1], filenames[i]) < 0 || strlen(path) >= sizeof path) return 2;
        status = jpl_open(path, &ephemeris);
        if (status != JPL_OK) {
            fprintf(stderr, "%s: %s\n", filenames[i], jpl_status_string(status));
            return 1;
        }
        header = jpl_get_header(ephemeris);
        if (header == NULL || header->record_double_count < 3 || jpl_record_count(ephemeris) == 0) {
            fprintf(stderr, "%s: invalid header\n", filenames[i]);
            jpl_close(ephemeris);
            return 1;
        }
        record = malloc(header->record_double_count * sizeof *record);
        if (record == NULL ||
            jpl_read_record(ephemeris, 0, record, header->record_double_count) != JPL_OK ||
            record[0] < header->start_jd || record[1] <= record[0]) {
            fprintf(stderr, "%s: could not decode first coefficient record\n", filenames[i]);
            free(record);
            jpl_close(ephemeris);
            return 1;
        }
        free(record);
        jpl_close(ephemeris);
    }
    puts("all supplied JPL DE binaries opened successfully");
    return 0;
}
