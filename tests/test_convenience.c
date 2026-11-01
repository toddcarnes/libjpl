/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_convenience.h"
#include "jpl_frames.h"
#include "jpl_private.h"
#include "jpl_test_util.h"
#include "numerical_reference.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    /* 1 AU/day = AU_KM/86400 km/s ~= 1731.45683; 0.01 -> ~17.3145683,
     * checked absolutely so constant typos fail. Converters return status
     * and leave outputs unchanged on failure. */
    double v[3]={0.01,0,0}, out[3]; JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(v,out)==JPL_OK);
    JPL_TEST_CHECK(fabs(out[0]-17.3145683) < 1e-6);
    { double au=0, expect = JPL_TEST_AU_KM/JPL_TEST_AU_KM;
      JPL_TEST_CHECK(jpl_km_to_au(JPL_TEST_AU_KM,&au)==JPL_OK && au==expect);
      JPL_TEST_CHECK(jpl_km_to_au(JPL_TEST_AU_KM,&au)==JPL_OK && au>0.999&&au<1.001); }
    /* Status returns, unchanged outputs, aliasing: */
    {
        double w[3]={1.0,2.0,3.0}, before[3]={1.0,2.0,3.0}, o2[3]={9.0,9.0,9.0}, keep[3]={9.0,9.0,9.0};
        double a=7.0, acopy=7.0, nanv[3]={0.0, NAN, 0.0};
        JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(NULL,o2)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(o2[0]==keep[0]&&o2[1]==keep[1]&&o2[2]==keep[2]);
        JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(w,NULL)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(nanv,o2)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(o2[0]==keep[0]&&o2[1]==keep[1]&&o2[2]==keep[2]);
        JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(w,w)==JPL_OK);
        JPL_TEST_CHECK(w[0]==before[0]*JPL_TEST_AU_KM/JPL_TEST_DAYSEC);
        JPL_TEST_CHECK(jpl_km_to_au(1.0,NULL)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_km_to_au(NAN,&a)==JPL_ERROR_ARGUMENT && a==acopy);
        JPL_TEST_CHECK(jpl_km_to_au(INFINITY,&a)==JPL_ERROR_ARGUMENT && a==acopy);
        JPL_TEST_CHECK(jpl_km_to_au(JPL_TEST_AU_KM,&a)==JPL_OK && a==1.0);
        /* Unrepresentable arithmetic: huge AU/day overflows km/s. */
        { double huge[3]={DBL_MAX,0.0,0.0}, o3[3]={4.0,5.0,6.0};
          JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(huge,o3)==JPL_ERROR_NUMERIC);
          JPL_TEST_CHECK(o3[0]==4.0&&o3[1]==5.0&&o3[2]==6.0); }
        /* Modern/file-AU round trip: file AU converts through the modern
         * AU but its header value is what it is. */
        { double file_au=JPL_TEST_NONMODERN_AU_KM, mod=0.0;
          JPL_TEST_CHECK(jpl_km_to_au(file_au,&mod)==JPL_OK);
          JPL_TEST_CHECK(fabs(mod - file_au/JPL_TEST_AU_KM) < 1e-15 && mod != 1.0); }
    }
    double st[6];
    static const double jd_null[2] = {2451545.0, 0.0};
    JPL_TEST_CHECK(jpl_planet_position(NULL,jd_null,1,11,st)==JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_moon_position(NULL,jd_null,st)==JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_planet_position(NULL,jd_null,99,11,st)==JPL_ERROR_ARGUMENT);
    /* Convenience calls require an ICRF-aligned DENUM. Synthetic mocks
     * that test successful ICRS output must declare DENUM 441 explicitly;
     * zero-DENUM mocks classify as UNKNOWN. */
    {
        const char *path = "jpl_test_convenience_frame.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const double jd[2] = {2451545.5, 0.0};
        double got[6] = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
        double keep[6] = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
        double raw[6];
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 3, 2, 1);   /* EMB */
        test_put_ptr(rec, 9, 9, 2, 1);   /* geocentric Moon */
        test_put_ptr(rec, 10, 15, 2, 1); /* Sun */
        test_put_ptr(rec, 12, 441, 0, 0); /* DENUM 441 -> ICRF */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES] = {0}; test_put_f64(cc, 1.0); test_put_f64(cc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(cc, 1, sizeof cc, f) == sizeof cc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0); test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 2 * 8, JPL_TEST_AU_KM);
        test_put_f64(rec + 8 * 8, 384400.0);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_header(e)->reference_frame == JPL_FRAME_ICRF);
        JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_SUN, JPL_EARTH_MOON_BARYCENTER, got) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_SUN, JPL_EARTH_MOON_BARYCENTER, raw) == JPL_OK);
        { size_t k; int differ = 0; for (k = 0; k < 6; ++k) if (got[k] != keep[k]) differ = 1; JPL_TEST_CHECK(differ); }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Mock ephemeris: EMB=(1 AU,0,0), geocentric Moon=(384400 km,0,0),
     * Sun=origin, zero velocities. Record of 357 doubles. */
    {
        const char *path="jpl_test_convenience.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM, MOON_KM=384400.0;
        static const double jd[2]={2451545.5, 0.0};
        double moon_earth[6], earth_sun[6], moon_sun[6], sum[6];
        double expect_moon, expect_earth, d;
        size_t i;
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        test_put_ptr(rec,2,3,2,1);   /* EMB */
        test_put_ptr(rec,9,9,2,1);   /* geocentric Moon */
        test_put_ptr(rec,10,15,2,1); /* Sun */
        test_put_ptr(rec,12,441,0,0); /* DENUM 441 -> ICRF, not zero */
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
        {
            jpl_apparent_ecliptic_result position = {7.0, 8.0, 9.0, 10.0, 11.0};
            const jpl_apparent_ecliptic_result keep = {7.0, 8.0, 9.0, 10.0, 11.0};
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_MOON, &position) == JPL_OK);
            JPL_TEST_CHECK(isfinite(position.longitude_deg) && position.longitude_deg >= 0.0 && position.longitude_deg < 360.0);
            JPL_TEST_CHECK(isfinite(position.latitude_deg) && fabs(position.latitude_deg) <= 90.0);
            JPL_TEST_CHECK(fabs(position.range_au - MOON_KM / AU_KM) < 1e-9);
            JPL_TEST_CHECK(isfinite(position.longitude_rate_deg_per_day));
            JPL_TEST_CHECK(isfinite(position.range_rate_km_per_s));
            JPL_TEST_CHECK(fabs(position.range_rate_km_per_s) < 1e-3);
            /* The forward neighbor crosses coverage; a backward rate remains available. */
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, (const double[2]){2451545.0, 0.9999},
                                                          JPL_MOON, &position) == JPL_OK);
            JPL_TEST_CHECK(isfinite(position.longitude_rate_deg_per_day));
            JPL_TEST_CHECK(isfinite(position.range_rate_km_per_s));
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_SUN, &position) == JPL_OK);
            JPL_TEST_CHECK(position.longitude_deg >= 0.0 && position.longitude_deg < 360.0 && position.range_au > 0.9);
            position = keep;
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_EARTH, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_SSB, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_EARTH_MOON_BARYCENTER, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_NUTATIONS, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_LIBRATIONS, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(&position, &keep, sizeof keep) == 0);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(NULL, jd, JPL_MOON, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, NULL, JPL_MOON, &position) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_MOON, NULL) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(&position, &keep, sizeof keep) == 0);
        }
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
    /* An artificial target close to the Sun but outside its apparent
     * disk distinguishes finite-source bending from aberration alone. */
    {
        const char *path = "jpl_test_convenience_bending.bin";
        enum { N = 360, BYTES = N * 8 };
        static unsigned char hdr[BYTES];
        static double rec[N];
        jpl_test_mock mock;
        jpl_ephemeris *e = NULL;
        jpl_date jd = {2451545.5, 0.0};
        jpl_apparent_ecliptic_result out_pos;
        jpl_state old_state;
        double old_ecliptic[3], lon_old, difference;
        memset(hdr, 0, sizeof hdr);
        memset(rec, 0, sizeof rec);
        memset(&mock, 0, sizeof mock);
        test_put_ptr(hdr, 0, 3, 2, 1);   /* Mercury: x/y slopes */
        test_put_ptr(hdr, 2, 9, 2, 1);   /* EMB = Earth: Moon is zero */
        test_put_ptr(hdr, 9, 15, 2, 1);  /* geocentric Moon */
        test_put_ptr(hdr, 10, 21, 2, 1); /* Sun at the origin */
        rec[0] = 2451545.0;
        rec[1] = 2451546.0;
        rec[2] = -0.5 * JPL_TEST_AU_KM;
        rec[3] = 0.005 * JPL_TEST_AU_KM;
        rec[4] = 0.01 * JPL_TEST_AU_KM;
        rec[5] = -0.0025 * JPL_TEST_AU_KM;
        rec[8] = JPL_TEST_AU_KM;
        mock.record_doubles = N;
        mock.advertised_records = 1;
        mock.denum = 441;
        mock.constant_count = 2;
        mock.start[0] = 2451545.0;
        mock.end[0] = 2451546.0;
        mock.step_days = 1.0;
        mock.au_km = JPL_TEST_AU_KM;
        mock.emrat = JPL_TEST_MASS_RATIO;
        mock.header = hdr;
        mock.records = rec;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e, &jd, JPL_MERCURY, JPL_EARTH, &old_state) == JPL_OK);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(&old_state, &jd, old_ecliptic) == JPL_OK);
        lon_old = atan2(old_ecliptic[1], old_ecliptic[0]);
        JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, &jd, JPL_MERCURY, &out_pos) == JPL_OK);
        {
            double new_lon_rad = out_pos.longitude_deg * (acos(-1.0) / 180.0);
            double diff = new_lon_rad - lon_old;
            difference = atan2(sin(diff), cos(diff));
        }
        JPL_TEST_CHECK(out_pos.range_au > 1.5);
        JPL_TEST_CHECK(out_pos.longitude_rate_deg_per_day > 0.1);
        JPL_TEST_CHECK(out_pos.range_rate_km_per_s < -1.0);
        {
            jpl_apparent_ecliptic_result near_end;
            jpl_date near_end_tt = {2451545.0, 0.9999};
            JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, &near_end_tt,
                                                           JPL_MERCURY, &near_end) == JPL_OK);
            JPL_TEST_CHECK(near_end.longitude_rate_deg_per_day > 0.1);
            JPL_TEST_CHECK(fabs(near_end.longitude_rate_deg_per_day -
                                out_pos.longitude_rate_deg_per_day) < 0.03);
            JPL_TEST_CHECK(near_end.range_rate_km_per_s < -1.0);
        }
        jpl_close(e);
        e = NULL;
        /* A positive-x target crossing y=0 tests signed longitude speed
         * across the 360-to-0 degree normalization boundary. */
        rec[2] = 2.0 * JPL_TEST_AU_KM;
        rec[3] = 0.0;
        rec[4] = 0.00005 * JPL_TEST_AU_KM;
        rec[5] = 0.005 * JPL_TEST_AU_KM;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, &jd, JPL_MERCURY, &out_pos) == JPL_OK);
        JPL_TEST_CHECK(out_pos.longitude_deg < 0.02 || out_pos.longitude_deg > 359.98);
        JPL_TEST_CHECK(out_pos.longitude_rate_deg_per_day > 0.1 &&
                       out_pos.longitude_rate_deg_per_day < 5.0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Reception-vs-emission regression: EMB y(t) has a quadratic Chebyshev
     * term so observer velocity differs between tdb and tdb-lt. Sun seen
     * from EMB gives a transverse (-x sight line, y velocity) aberration
     * lever; the convenience output must match the reception-epoch
     * jpl_apply_aberration composition within 1e-12 and differ from
     * the stale emission-epoch composition. */
    {
        const char *path="jpl_test_convenience_epoch.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM;
        static const double jd[2]={2451545.5, 0.0};
        double tdb[2], lt, lt_state[6], vc_rec[6], vc_emi[6], expect[6], got[6];
        double emi[2], vsun[6], sdist;
        size_t i;
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        /* EMB (idx 2): start=3 (1-based), 3 coeffs/comp, 1 subinterval:
         * x: c0,c1,c2 ; y: c0,c1,c2 ; z: c0,c1,c2  (9 doubles at idx 2..10) */
        test_put_ptr(rec,2,3,3,1);
        /* Sun (idx 10): zero coeffs at different slot; use start=12, 2 coeffs, 1 sub */
        test_put_ptr(rec,10,12,2,1);
        test_put_ptr(rec,12,441,0,0); /* DENUM 441 -> ICRF, not zero */
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
        JPL_TEST_CHECK(jpl_tt_to_tdb(jd,tdb)==JPL_OK);
        /* Convenience under test: Sun seen from EMB. */
        JPL_TEST_CHECK(jpl_planet_position(e,jd,JPL_SUN,JPL_EARTH_MOON_BARYCENTER,got)==JPL_OK);
        /* Manual composition with reception-epoch observer velocity. */
        JPL_TEST_CHECK(jpl_get_state_lighttime(e,tdb,JPL_SUN,JPL_EARTH_MOON_BARYCENTER,lt_state,&lt)==JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e,tdb,JPL_EARTH_MOON_BARYCENTER,0,vc_rec)==JPL_OK);
        JPL_TEST_CHECK(jpl_date_add_days(tdb,-lt,emi)==JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e,emi,JPL_EARTH_MOON_BARYCENTER,0,vc_emi)==JPL_OK);
        /* Observer velocities must actually differ (else test is vacuous). */
        { double dv=0.0; for(i=3;i<6;++i){double d=vc_rec[i]-vc_emi[i]; dv+=d*d;} dv=sqrt(dv);
          JPL_TEST_CHECK(dv > 1e-9); }
        JPL_TEST_CHECK(jpl_get_state(e,tdb,JPL_EARTH_MOON_BARYCENTER,JPL_SUN,vsun)==JPL_OK);
        sdist = test_norm3(vsun);
        JPL_TEST_CHECK(sdist > 0.5);
        { double vobs[3]={vc_rec[3],vc_rec[4],vc_rec[5]};
          JPL_TEST_CHECK(jpl_apply_aberration(lt_state,vobs,sdist,expect)==JPL_OK); }
        for(i=0;i<6;++i) JPL_TEST_CHECK(fabs(got[i]-expect[i]) < 1e-12);
        /* And it must NOT match the stale emission-epoch composition. */
        { double stale[6], vobs_e[3]={vc_emi[3],vc_emi[4],vc_emi[5]}, mdiff=0.0;
          JPL_TEST_CHECK(jpl_apply_aberration(lt_state,vobs_e,sdist,stale)==JPL_OK);
          for(i=0;i<6;++i){double d=fabs(got[i]-stale[i]); if(d>mdiff) mdiff=d;}
          JPL_TEST_CHECK(mdiff > 1e-9); }
        /* NULL converter guards: status returns, outputs untouched. */
        { double keep[6]; size_t k; for(k=0;k<6;++k) keep[k]=got[k];
          JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(NULL,got)==JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(jpl_au_per_day_to_km_per_s(vc_rec,NULL)==JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(jpl_km_to_au(1.0,NULL)==JPL_ERROR_ARGUMENT);
          for(k=0;k<6;++k) JPL_TEST_CHECK(got[k]==keep[k]); }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path)==0);
    }
    /* Status propagation: aberration failure inside the convenience call
     * (superluminal observer velocity from a fast-moving-center mock)
     * returns the aberration status with the caller output unchanged. */
    {
        const char *path="jpl_test_convenience_aberr.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL;
        const double AU_KM=JPL_TEST_AU_KM;
        const double C_AU_DAY=JPL_TEST_C_KM_PER_SECOND*JPL_TEST_DAYSEC/JPL_TEST_AU_KM;
        static const double jd[2]={2451545.5, 0.0};
        double keep[6]={11.0,22.0,33.0,44.0,55.0,66.0}, got[6];
        memcpy(got, keep, sizeof got);
        test_mock_header(rec,2451545.0,2451546.0,1.0,2);
        /* EMB (idx 2): x constant at 1 AU, y slope superluminal, z zero. */
        test_put_ptr(rec,2,3,2,1);
        test_put_ptr(rec,9,9,2,1);   /* geocentric Moon: zeros */
        test_put_ptr(rec,10,15,2,1); /* Sun: zeros */
        test_put_ptr(rec,12,441,0,0); /* DENUM 441 -> ICRF, not zero */
        f=fopen(path,"wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES]={0}; test_put_f64(cc,1.0); test_put_f64(cc+8,2.0);
          JPL_TEST_CHECK(fwrite(cc,1,sizeof cc,f)==sizeof cc); }
        memset(rec,0,sizeof rec);
        test_put_f64(rec+0*8,2451545.0); test_put_f64(rec+1*8,2451546.0);
        test_put_f64(rec+2*8,AU_KM); test_put_f64(rec+3*8,0.0);       /* EMB x */
        test_put_f64(rec+4*8,0.0); test_put_f64(rec+5*8,4.0*C_AU_DAY*AU_KM); /* EMB y slope: 4c */
        test_put_f64(rec+6*8,0.0); test_put_f64(rec+7*8,0.0);         /* EMB z */
        test_put_f64(rec+8*8,0.0); test_put_f64(rec+9*8,0.0);         /* Moon */
        JPL_TEST_CHECK(fwrite(rec,1,JPL_TEST_HEADER_BYTES,f)==JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f)==0);
        JPL_TEST_CHECK(jpl_open(path,&e)==JPL_OK);
        /* Sun seen from the fast EMB: observer velocity is 4c, so the
         * composed aberration reports ARGUMENT and st stays unchanged. */
        JPL_TEST_CHECK(jpl_planet_position(e,jd,JPL_SUN,JPL_EARTH_MOON_BARYCENTER,got)==JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got)==0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path)==0);
    }
    /* DENUM matrix: core states succeed in their native frame for
     * 102/200/202/441/999; convenience requires ICRF, so legacy/unknown
     * frames return UNSUPPORTED with outputs preserved. */
    {
        static const struct { int denum; jpl_reference_frame frame; } cases[] = {
            {102, JPL_FRAME_DYNAMICAL_1950},
            {200, JPL_FRAME_DYNAMICAL_2000},
            {202, JPL_FRAME_DYNAMICAL_2000},
            {441, JPL_FRAME_ICRF},
            {999, JPL_FRAME_UNKNOWN},
        };
        size_t c;
        for (c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
            const char *path = "jpl_test_convenience_denum.bin";
            unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
            FILE *f;
            jpl_ephemeris *e = NULL;
            static const double jd[2] = {2451545.5, 0.0};
            double core[6], conv[6] = {3.0, 5.0, 7.0, 9.0, 11.0, 13.0};
            double keep[6] = {3.0, 5.0, 7.0, 9.0, 11.0, 13.0};
            test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
            test_put_ptr(rec, 0, 3, 2, 1);   /* Mercury body coefficients */
            test_put_ptr(rec, 2, 9, 2, 1);   /* EMB leg for Earth derivation */
            test_put_ptr(rec, 9, 15, 2, 1);  /* geocentric Moon leg */
            test_put_ptr(rec, 10, 21, 2, 1); /* Sun leg */
            test_put_ptr(rec, 12, (uint32_t)cases[c].denum, 0, 0);
            f = fopen(path, "wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
            { unsigned char cc[JPL_TEST_HEADER_BYTES] = {0}; test_put_f64(cc, 1.0); test_put_f64(cc + 8, 2.0);
              JPL_TEST_CHECK(fwrite(cc, 1, sizeof cc, f) == sizeof cc); }
            memset(rec, 0, sizeof rec);
            test_put_f64(rec + 0 * 8, 2451545.0); test_put_f64(rec + 1 * 8, 2451546.0);
            test_put_f64(rec + 2 * 8, JPL_TEST_AU_KM);
            test_put_f64(rec + 8 * 8, JPL_TEST_AU_KM);
            test_put_f64(rec + 14 * 8, JPL_TEST_AU_KM);
            /* A distinct Sun keeps the finite-source direction defined. */
            test_put_f64(rec + 20 * 8, 2.0 * JPL_TEST_AU_KM);
            JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f) == 0);
            JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_header(e)->ephemeris_number == cases[c].denum);
            JPL_TEST_CHECK(jpl_get_header(e)->reference_frame == cases[c].frame);
            /* Core native-frame read succeeds for every DENUM. */
            JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_MERCURY, JPL_SUN, core) == JPL_OK);
            if (cases[c].frame == JPL_FRAME_ICRF) {
                JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_SUN, conv) == JPL_OK);
            } else {
                JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_SUN, conv) == JPL_ERROR_UNSUPPORTED);
                JPL_TEST_CHECK(memcmp(conv, keep, sizeof conv) == 0);
            }
            {
                jpl_apparent_ecliptic_result ecliptic = {7.0, 8.0, 9.0, 10.0, 11.0};
                const jpl_apparent_ecliptic_result keep_ecliptic = {7.0, 8.0, 9.0, 10.0, 11.0};
                if (cases[c].frame == JPL_FRAME_ICRF) {
                    JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_MERCURY, &ecliptic) == JPL_OK);
                } else {
                    JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_MERCURY, &ecliptic) == JPL_ERROR_UNSUPPORTED);
                    JPL_TEST_CHECK(memcmp(&ecliptic, &keep_ecliptic, sizeof ecliptic) == 0);
                    JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, (const double[2]){2451540.0, 0.0},
                                                                 JPL_MERCURY, &ecliptic) == JPL_ERROR_RANGE);
                }
            }
            /* Frame eligibility precedes the zero shortcut: identical-body
             * convenience on a legacy frame reports UNSUPPORTED, never a
             * zero vector that would imply support. */
            memcpy(conv, keep, sizeof conv);
            if (cases[c].frame == JPL_FRAME_ICRF) {
                JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_MERCURY, conv) == JPL_OK);
                { size_t k; for (k = 0; k < 6; ++k) JPL_TEST_CHECK(conv[k] == 0.0); }
            } else {
                JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_MERCURY, conv) == JPL_ERROR_UNSUPPORTED);
                JPL_TEST_CHECK(memcmp(conv, keep, sizeof conv) == 0);
            }
            jpl_close(e);
            JPL_TEST_CHECK(remove(path) == 0);
        }
    }
    /* Centers 0 and 12 are equivalent. A no-Sun SSB-observer mock
     * (Sun coefficients absent, so Sun work must not be attempted) matches
     * the committed DE441 fixture behavior exercised below. */
    {
        const char *path = "jpl_test_convenience_ssb.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const double jd[2] = {2451545.5, 0.0};
        double via0[6], via12[6], lt0, lt12, raw0[6], raw12[6];
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 0, 3, 2, 1);   /* Mercury body coefficients */
        test_put_ptr(rec, 12, 441, 0, 0); /* DENUM 441 -> ICRF */
        /* Sun pointer (idx 10) stays absent: the SSB shortcut must not
         * evaluate Sun data. */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES] = {0}; test_put_f64(cc, 1.0); test_put_f64(cc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(cc, 1, sizeof cc, f) == sizeof cc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0); test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 2 * 8, JPL_TEST_AU_KM);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, 0, via0) == JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_SOLAR_SYSTEM_BARYCENTER, via12) == JPL_OK);
        { size_t k; for (k = 0; k < 6; ++k) JPL_TEST_CHECK(via0[k] == via12[k]); }
        /* SSB-observer convenience equals the light-time state: no
         * aberration is applied to a zero-velocity observer. */
        { double tdb[2]; JPL_TEST_CHECK(jpl_tt_to_tdb(jd, tdb) == JPL_OK);
          JPL_TEST_CHECK(jpl_get_state_lighttime(e, tdb, JPL_MERCURY, 0, raw0, &lt0) == JPL_OK);
          JPL_TEST_CHECK(jpl_get_state_lighttime(e, tdb, JPL_MERCURY, JPL_SOLAR_SYSTEM_BARYCENTER, raw12, &lt12) == JPL_OK);
          { size_t k; for (k = 0; k < 6; ++k) JPL_TEST_CHECK(via0[k] == raw0[k]); }
          { size_t k; for (k = 0; k < 6; ++k) JPL_TEST_CHECK(via12[k] == raw12[k]); } }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* EMB/EMB with EMB coefficients but no Sun returns zero OK
     * without Sun work; invalid dates on that same shortcut report
     * RANGE/ARGUMENT before any dataset question. */
    {
        const char *path = "jpl_test_convenience_emb.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const double jd[2] = {2451545.5, 0.0};
        static const double jd_bad[2] = {2451540.0, 0.0};
        static const double jd_nan[2] = {2451545.5, NAN};
        double got[6] = {4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
        double keep[6] = {4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
        size_t k;
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 3, 2, 1);   /* EMB leg only; Sun stays absent */
        test_put_ptr(rec, 12, 441, 0, 0); /* DENUM 441 -> ICRF */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES] = {0}; test_put_f64(cc, 1.0); test_put_f64(cc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(cc, 1, sizeof cc, f) == sizeof cc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0); test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 2 * 8, JPL_TEST_AU_KM);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_EARTH_MOON_BARYCENTER, got) == JPL_OK);
        for (k = 0; k < 6; ++k) JPL_TEST_CHECK(got[k] == 0.0);
        memcpy(got, keep, sizeof got);
        JPL_TEST_CHECK(jpl_planet_position(e, jd_bad, JPL_EARTH_MOON_BARYCENTER, JPL_EARTH_MOON_BARYCENTER, got) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got) == 0);
        JPL_TEST_CHECK(jpl_planet_position(e, jd_nan, JPL_EARTH_MOON_BARYCENTER, JPL_EARTH_MOON_BARYCENTER, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got) == 0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* On the committed DE441 fixture, centers 0 and 12 agree
     * exactly through the ICRF convenience path. The fixture ships with
     * the repo; probe the standard relative locations and skip only when
      * the binary is genuinely absent. */
    {
        static const char *candidates[] = {
            "../../tests/data/de441_2000_2010.eph",
            "tests/data/de441_2000_2010.eph",
            "../tests/data/de441_2000_2010.eph",
        };
        const char *found = NULL;
        size_t c;
        for (c = 0; c < sizeof candidates / sizeof candidates[0]; ++c) {
            FILE *probe = fopen(candidates[c], "rb");
            if (probe != NULL) { fclose(probe); found = candidates[c]; break; }
        }
        if (found == NULL) {
            puts("convenience note: DE441 fixture absent, skipping 0/12 fixture check");
        } else {
            jpl_ephemeris *e = NULL;
            static const double jd[2] = {2451545.0, 0.5};
            double via0[6], via12[6];
            size_t k;
            JPL_TEST_CHECK(jpl_open(found, &e) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_header(e)->ephemeris_number == 441);
            JPL_TEST_CHECK(jpl_get_header(e)->reference_frame == JPL_FRAME_ICRF);
            JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, 0, via0) == JPL_OK);
            JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_MERCURY, JPL_SOLAR_SYSTEM_BARYCENTER, via12) == JPL_OK);
            for (k = 0; k < 6; ++k) JPL_TEST_CHECK(via0[k] == via12[k]);
            jpl_close(e);
        }
    }
    {
        const char *path = "jpl_test_convenience_noemb.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const double jd[2] = {2451545.5, 0.0};
        static const double jd_bad[2] = {2451540.0, 0.0};
        double st_out[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        double keep[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        double core[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        double core_keep[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 10, 3, 2, 1);   /* Sun only; EMB/Moon absent */
        test_put_ptr(rec, 12, 441, 0, 0); /* DENUM 441 -> ICRF */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char cc[JPL_TEST_HEADER_BYTES] = {0}; test_put_f64(cc, 1.0); test_put_f64(cc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(cc, 1, sizeof cc, f) == sizeof cc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0); test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 2 * 8, JPL_TEST_AU_KM);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES); JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        /* Core: missing EMB dataset on a valid date is UNSUPPORTED. */
        JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_EARTH_MOON_BARYCENTER, 0, core) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(memcmp(core, core_keep, sizeof core) == 0);
        /* Convenience: identical-body Earth/Earth shortcut validates the
         * required EMB leg first, so it reports UNSUPPORTED here. */
        JPL_TEST_CHECK(jpl_planet_position(e, jd, JPL_EARTH, JPL_EARTH, st_out) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(memcmp(st_out, keep, sizeof st_out) == 0);
        /* Invalid dates on the same shortcut report RANGE, not the
         * dataset status. */
        JPL_TEST_CHECK(jpl_planet_position(e, jd_bad, JPL_EARTH, JPL_EARTH, st_out) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(memcmp(st_out, keep, sizeof st_out) == 0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Pinned apparent oracle through the convenience wrapper. The
     * linear-mock legs below reproduce case 0 of the independent
     * analytic oracle (P/V target, O/W observer, Sun at origin); the
     * wrapper must match the pinned state and delay path. */
    {
        const jpl_apparent_reference *ref = &jpl_apparent_references[0];
        const char *path = "jpl_test_convenience_oracle.bin";
        enum { CDBL = 360, CBYTES = CDBL * 8 };
        static unsigned char hdr[CBYTES];
        static unsigned char con[CBYTES];
        static unsigned char dat[CBYTES];
        FILE *f;
        jpl_ephemeris *e = NULL;
        double conv[6];
        size_t k;
        static const double P[3] = {1.0, 0.4, -0.2};
        static const double V[3] = {0.005, -0.003, 0.001};
        static const double O[3] = {0.1, -0.2, 0.03};
        static const double W[3] = {0.002, 0.004, -0.001};
        memset(hdr, 0, sizeof hdr);
        memset(con, 0, sizeof con);
        memset(dat, 0, sizeof dat);
        test_mock_header(hdr, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(hdr, 3, 3, 2, 1);
        test_put_ptr(hdr, 2, 9, 2, 1);
        test_put_ptr(hdr, 10, 15, 2, 1);
        test_put_ptr(hdr, 12, 441, 0, 0);
        test_put_f64(con, 1.0);
        test_put_f64(con + 8, 2.0);
        test_put_f64(dat + 0 * 8, 2451545.0);
        test_put_f64(dat + 1 * 8, 2451546.0);
        for (k = 0; k < 3; ++k) {
            test_put_f64(dat + (2 + k * 2) * 8, P[k] * JPL_TEST_AU_KM);
            test_put_f64(dat + (3 + k * 2) * 8, (V[k] * 0.5) * JPL_TEST_AU_KM);
            test_put_f64(dat + (8 + k * 2) * 8, O[k] * JPL_TEST_AU_KM);
            test_put_f64(dat + (9 + k * 2) * 8, (W[k] * 0.5) * JPL_TEST_AU_KM);
        }
        f = fopen(path, "wb");
        JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
        JPL_TEST_CHECK(fwrite(con, 1, sizeof con, f) == sizeof con);
        JPL_TEST_CHECK(fwrite(dat, 1, sizeof dat, f) == sizeof dat);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_apparent_reference_count == 2);
        JPL_TEST_CHECK(jpl_planet_position(e, ref->tt, JPL_MARS, JPL_EARTH_MOON_BARYCENTER, conv) == JPL_OK);
        for (k = 0; k < 6; ++k) {
            JPL_TEST_CHECK(isfinite(conv[k]));
            JPL_TEST_CHECK(fabs(conv[k] - ref->state[k]) < 1e-9);
        }
        /* A finite but unrepresentable date pair reports NUMERIC
         * with outputs unchanged. */
        {
            static const double overflow[2] = {DBL_MAX, DBL_MAX};
            double keep[6], got[6];
            size_t q;
            for (q = 0; q < 6; ++q) { keep[q] = 3.0; got[q] = 3.0; }
            JPL_TEST_CHECK(jpl_planet_position(e, overflow, JPL_MARS, JPL_EARTH_MOON_BARYCENTER, got) == JPL_ERROR_NUMERIC);
            for (q = 0; q < 6; ++q) JPL_TEST_CHECK(got[q] == keep[q]);
            JPL_TEST_CHECK(jpl_moon_position(NULL, overflow, got) == JPL_ERROR_ARGUMENT);
        }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("convenience tests passed"); return 0;
}
