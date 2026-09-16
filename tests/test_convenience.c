#include "jpl_convenience.h"
#include "jpl_test_util.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    /* 1 AU/day = AU_KM/86400 km/s ~= 1731.45683; 0.01 -> ~17.3145683,
     * checked absolutely so constant typos fail. */
    double v[3]={0.01,0,0}, out[3]; jpl_au_per_day_to_km_per_s(v,out);
    assert(fabs(out[0]-17.3145683) < 1e-6);
    double au=0; jpl_km_to_au(JPL_TEST_AU_KM,&au); assert(au>0.999&&au<1.001);
    double st[6];
    assert(jpl_planet_position(NULL,2451545.0,1,11,st)==JPL_ERROR_ARGUMENT);
    assert(jpl_moon_position(NULL,2451545.0,st)==JPL_ERROR_ARGUMENT);
    assert(jpl_planet_position(NULL,2451545.0,99,11,st)==JPL_ERROR_ARGUMENT);
    /* Mock ephemeris: EMB=(1 AU,0,0), geocentric Moon=(384400 km,0,0),
     * Sun=origin, zero velocities. Record of 357 doubles. */
    {
        const char *path="jpl_test_convenience.bin";
        unsigned char rec[JPL_TEST_RECORD_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM, MOON_KM=384400.0, jd=2451545.5;
        double moon_earth[6], earth_sun[6], moon_sun[6], sum[6];
        double expect_moon, expect_earth, d;
        size_t i;
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        test_put_ptr(rec,2,3,2,1);   /* EMB */
        test_put_ptr(rec,9,9,2,1);   /* geocentric Moon */
        test_put_ptr(rec,10,15,2,1); /* Sun */
        f=fopen(path,"wb"); assert(f); assert(fwrite(rec,1,sizeof rec,f)==sizeof rec);
        memset(rec,0,sizeof rec); test_put_f64(rec,1.0); test_put_f64(rec+8,2.0);
        assert(fwrite(rec,1,sizeof rec,f)==sizeof rec);
        memset(rec,0,sizeof rec);
        test_put_f64(rec+0*8,2451545.0); test_put_f64(rec+1*8,2451546.0);
        test_put_f64(rec+2*8,AU_KM);                       /* EMB x */
        test_put_f64(rec+8*8,MOON_KM);                     /* Moon x */
        assert(fwrite(rec,1,sizeof rec,f)==sizeof rec); assert(fclose(f)==0);
        assert(jpl_open(path,&e)==JPL_OK);
        /* Validation: invalid planet/center codes rejected. */
        assert(jpl_planet_position(e,jd,0,11,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,12,11,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,14,0,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,15,0,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,99,11,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,-1,11,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,1,14,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,1,15,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,1,-1,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,1,16,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_planet_position(e,jd,1,99,st)==JPL_ERROR_ARGUMENT);
        assert(jpl_moon_position(e,jd,moon_earth)==JPL_OK);
        assert(jpl_planet_position(e,jd,JPL_EARTH,JPL_SUN,earth_sun)==JPL_OK);
        assert(jpl_planet_position(e,jd,JPL_MOON,JPL_SUN,moon_sun)==JPL_OK);
        expect_moon = MOON_KM/AU_KM;
        d = test_norm3(moon_earth);
        assert(fabs(d-expect_moon) < 1e-9);
        assert(fabs(moon_earth[0]-expect_moon) < 1e-9); /* x-axis invariant under Rx */
        expect_earth = 1.0 - expect_moon/(1.0+JPL_TEST_MASS_RATIO);
        assert(fabs(earth_sun[0]-expect_earth) < 1e-9);
        /* DE data is already ICRS-equatorial: the convenience output must
         * match the raw geometric vector on the invariant x-axis (aberration
         * is transverse, so x is untouched for this x-axis geometry). */
        { double raw[6]; assert(jpl_get_state(e,jd,JPL_EARTH,JPL_SUN,raw)==JPL_OK);
          assert(fabs(earth_sun[0]-raw[0]) < 1e-9); }
        /* Vector identity: moon_sun == moon_earth + earth_sun (zero
         * velocities make the three legs consistent). */
        for (i=0;i<6;++i) sum[i]=moon_earth[i]+earth_sun[i];
        for (i=0;i<6;++i) assert(fabs(sum[i]-moon_sun[i]) < 1e-9);
        jpl_close(e);
        assert(remove(path)==0);
    }
    puts("convenience tests passed"); return 0;
}
