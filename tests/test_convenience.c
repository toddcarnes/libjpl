#include "jpl_convenience.h"
#include "jpl_frames.h"
#include "jpl_test_util.h"
#include <math.h>
#include <stdio.h>
int main(void) {
    /* 1 AU/day = AU_KM/86400 km/s ~= 1731.45683; 0.01 -> ~17.3145683,
     * checked absolutely so constant typos fail. */
    double v[3]={0.01,0,0}, out[3]; jpl_au_per_day_to_km_per_s(v,out);
    JPL_TEST_CHECK(fabs(out[0]-17.3145683) < 1e-6);
    double au=0; jpl_km_to_au(JPL_TEST_AU_KM,&au); JPL_TEST_CHECK(au>0.999&&au<1.001);
    double st[6];
    JPL_TEST_CHECK(jpl_planet_position(NULL,2451545.0,1,11,st)==JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_moon_position(NULL,2451545.0,st)==JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_planet_position(NULL,2451545.0,99,11,st)==JPL_ERROR_ARGUMENT);
    /* Mock ephemeris: EMB=(1 AU,0,0), geocentric Moon=(384400 km,0,0),
     * Sun=origin, zero velocities. Record of 357 doubles. */
    {
        const char *path="jpl_test_convenience.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM, MOON_KM=384400.0, jd=2451545.5;
        double moon_earth[6], earth_sun[6], moon_sun[6], sum[6];
        double expect_moon, expect_earth, d;
        size_t i;
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        test_put_ptr(rec,2,3,2,1);   /* EMB */
        test_put_ptr(rec,9,9,2,1);   /* geocentric Moon */
        test_put_ptr(rec,10,15,2,1); /* Sun */
        f=fopen(path,"wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES]={0}; test_put_f64(cc,1.0); test_put_f64(cc+8,2.0);
          JPL_TEST_CHECK(fwrite(cc,1,sizeof cc,f)==sizeof cc); }
        memset(rec,0,sizeof rec);
        test_put_f64(rec+0*8,2451545.0); test_put_f64(rec+1*8,2451546.0);
        test_put_f64(rec+2*8,AU_KM);                       /* EMB x */
        test_put_f64(rec+8*8,MOON_KM);                     /* Moon x */
        /* Uniform 2880-byte records: data block padded with zeros. */
        JPL_TEST_CHECK(fwrite(rec,1,JPL_TEST_HEADER_BYTES,f)==JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f)==0);
        JPL_TEST_CHECK(jpl_open(path,&e)==JPL_OK);
        /* Validation: invalid planet/center codes rejected. */
        JPL_TEST_CHECK(jpl_planet_position(e,jd,0,11,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,12,11,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,14,0,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,15,0,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,99,11,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,-1,11,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,1,14,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,1,15,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,1,-1,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,1,16,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,1,99,st)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_moon_position(e,jd,moon_earth)==JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,JPL_EARTH,JPL_SUN,earth_sun)==JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e,jd,JPL_MOON,JPL_SUN,moon_sun)==JPL_OK);
        expect_moon = MOON_KM/AU_KM;
        d = test_norm3(moon_earth);
        JPL_TEST_CHECK(fabs(d-expect_moon) < 1e-9);
        JPL_TEST_CHECK(fabs(moon_earth[0]-expect_moon) < 1e-9); /* x-axis invariant under Rx */
        expect_earth = 1.0 - expect_moon/(1.0+JPL_TEST_MASS_RATIO);
        JPL_TEST_CHECK(fabs(earth_sun[0]-expect_earth) < 1e-9);
        /* DE data is already ICRS-equatorial: the convenience output must
         * match the raw geometric vector on the invariant x-axis (aberration
         * is transverse, so x is untouched for this x-axis geometry). */
        { double raw[6]; JPL_TEST_CHECK(jpl_get_state(e,jd,JPL_EARTH,JPL_SUN,raw)==JPL_OK);
          JPL_TEST_CHECK(fabs(earth_sun[0]-raw[0]) < 1e-9); }
        /* Vector identity: moon_sun == moon_earth + earth_sun (zero
         * velocities make the three legs consistent). */
        for (i=0;i<6;++i) sum[i]=moon_earth[i]+earth_sun[i];
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(sum[i]-moon_sun[i]) < 1e-9);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path)==0);
    }
    /* Reception-vs-emission regression: EMB y(t) has a quadratic Chebyshev
     * term so observer velocity differs between tdb and tdb-lt. Sun seen
     * from EMB gives a transverse (-x sight line, y velocity) aberration
     * lever; the convenience output must match the reception-epoch
     * jpl_apply_aberration_full composition within 1e-12 and differ from
     * the stale emission-epoch composition. */
    {
        const char *path="jpl_test_convenience_epoch.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM, jd=2451545.5;
        double tdb, lt, lt_state[6], vc_rec[6], vc_emi[6], expect[6], got[6];
        double vsun[6], sdist;
        size_t i;
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        /* EMB (idx 2): start=3 (1-based), 3 coeffs/comp, 1 subinterval:
         * x: c0,c1,c2 ; y: c0,c1,c2 ; z: c0,c1,c2  (9 doubles at idx 2..10) */
        test_put_ptr(rec,2,3,3,1);
        /* Sun (idx 10): zero coeffs at different slot; use start=12, 2 coeffs, 1 sub */
        test_put_ptr(rec,10,12,2,1);
        f=fopen(path,"wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
        { unsigned char cc2[JPL_TEST_HEADER_BYTES]={0}; test_put_f64(cc2,1.0); test_put_f64(cc2+8,2.0);
          JPL_TEST_CHECK(fwrite(cc2,1,sizeof cc2,f)==sizeof cc2); }
        memset(rec,0,sizeof rec);
        test_put_f64(rec+0*8,2451545.0); test_put_f64(rec+1*8,2451546.0);
        /* EMB x coeffs (record doubles idx 2,3,4 -> bytes 16,24,32):
         * constant AU_KM in x, zero slope/curve. */
        test_put_f64(rec+2*8,AU_KM); test_put_f64(rec+3*8,0.0); test_put_f64(rec+4*8,0.0);
        /* EMB y coeffs (idx 5,6,7): c0=0, c1 = slope, c2 = curve (both km).
         * 3 coeffs give time-varying velocity; 2 would be constant. */
        { double slope=0.5*AU_KM; double curve=0.25*AU_KM;
          test_put_f64(rec+5*8,0.0); test_put_f64(rec+6*8,slope); test_put_f64(rec+7*8,curve); }
        /* EMB z coeffs (idx 8,9,10): zeros. */
        test_put_f64(rec+8*8,0.0); test_put_f64(rec+9*8,0.0); test_put_f64(rec+10*8,0.0);
        /* Sun coeffs (idx 11..16 for 3 comps x 2 coeffs): zeros. */
        test_put_f64(rec+11*8,0.0); test_put_f64(rec+12*8,0.0);
        test_put_f64(rec+13*8,0.0); test_put_f64(rec+14*8,0.0);
        test_put_f64(rec+15*8,0.0); test_put_f64(rec+16*8,0.0);
        /* Uniform 2880-byte records: data block padded with zeros. */
        JPL_TEST_CHECK(fwrite(rec,1,JPL_TEST_HEADER_BYTES,f)==JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f)==0);
        JPL_TEST_CHECK(jpl_open(path,&e)==JPL_OK);
        tdb = jpl_tt_to_tdb(jd);
        /* Convenience under test: Sun seen from EMB. */
        JPL_TEST_CHECK(jpl_planet_position(e,jd,JPL_SUN,JPL_EARTH_MOON_BARYCENTER,got)==JPL_OK);
        /* Manual composition with reception-epoch observer velocity. */
        JPL_TEST_CHECK(jpl_get_state_lighttime(e,tdb,JPL_SUN,JPL_EARTH_MOON_BARYCENTER,lt_state,&lt)==JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e,tdb,JPL_EARTH_MOON_BARYCENTER,0,vc_rec)==JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e,tdb-lt,JPL_EARTH_MOON_BARYCENTER,0,vc_emi)==JPL_OK);
        /* Observer velocities must actually differ (else test is vacuous). */
        { double dv=0.0; for(i=3;i<6;++i){double d=vc_rec[i]-vc_emi[i]; dv+=d*d;} dv=sqrt(dv);
          JPL_TEST_CHECK(dv > 1e-9); }
        JPL_TEST_CHECK(jpl_get_state(e,tdb,JPL_EARTH_MOON_BARYCENTER,JPL_SUN,vsun)==JPL_OK);
        sdist = test_norm3(vsun);
        JPL_TEST_CHECK(sdist > 0.5);
        { double vobs[3]={vc_rec[3],vc_rec[4],vc_rec[5]};
          jpl_apply_aberration_full(lt_state,vobs,sdist,expect); }
        for(i=0;i<6;++i) JPL_TEST_CHECK(fabs(got[i]-expect[i]) < 1e-12);
        /* And it must NOT match the stale emission-epoch composition. */
        { double stale[6], vobs_e[3]={vc_emi[3],vc_emi[4],vc_emi[5]}, mdiff=0.0;
          jpl_apply_aberration_full(lt_state,vobs_e,sdist,stale);
          for(i=0;i<6;++i){double d=fabs(got[i]-stale[i]); if(d>mdiff) mdiff=d;}
          JPL_TEST_CHECK(mdiff > 1e-9); }
        /* NULL converter guards: must not crash. */
        jpl_au_per_day_to_km_per_s(NULL,got);
        jpl_au_per_day_to_km_per_s(vc_rec,NULL);
        jpl_km_to_au(1.0,NULL);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path)==0);
    }
    puts("convenience tests passed"); return 0;
}
