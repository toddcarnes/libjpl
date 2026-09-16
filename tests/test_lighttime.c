#include "jpl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    double st[6] = {0};
    assert(jpl_get_state_lighttime(NULL, 2451545.0, JPL_EARTH, JPL_SUN, st, NULL) == JPL_ERROR_ARGUMENT);
    puts("lighttime arg test passed");
    return 0;
}
