#include "jpl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *file;
    jpl_ephemeris *ephemeris;
    const jpl_header *header;
    char line[256], label[80], date[80];
    size_t in_range_lines = 0, tested = 0;
    if (argc != 3) return 2;
    if (jpl_open(argv[1], &ephemeris) != JPL_OK) return 1;
    header = jpl_get_header(ephemeris);
    file = fopen(argv[2], "r");
    if (!file) { jpl_close(ephemeris); return 1; }
    while (fgets(line, sizeof line, file) != NULL) {
        int target, center, component, parsed;
        double expected, jd, state[6], actual, tolerance;
        parsed = sscanf(line, "%79s%79s%lf%d%d%d%lf", label, date, &jd, &target, &center, &component, &expected);
        if (parsed != 7) {
            if (sscanf(line, "%79s%d%d%d%lf", label, &target, &center, &component, &expected) != 5) continue;
            jd = strtod(strrchr(label, '-'), NULL);
        }
        if (jd < header->start_jd || jd > header->end_jd) continue;
        if ((in_range_lines++ % 32) != 0) continue;
        if (jpl_get_state(ephemeris, jd, target, center, state) != JPL_OK) {
            fprintf(stderr, "Could not evaluate %s\n", label); fclose(file); jpl_close(ephemeris); return 1;
        }
        actual = state[component - 1];
        tolerance = 5e-13 * fmax(1.0, fabs(expected));
        if (fabs(actual - expected) > tolerance) {
            fprintf(stderr, "%s JD %.1f target %d center %d component %d: got %.17g, expected %.17g\n", label, jd, target, center, component, actual, expected);
            fclose(file); jpl_close(ephemeris); return 1;
        }
        ++tested;
    }
    fclose(file); jpl_close(ephemeris);
    if (tested < 100) return 1;
    printf("validated %zu DE441 reference positions\n", tested);
    return 0;
}
