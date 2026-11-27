/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_fixture.h"
#include "jpl_test_util.h"
/* This file checks mmap and FILE clone identity, close order, and
 * sequential mmap shareability. Concurrent reads are in test_threads.c. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#if defined(_WIN32)
#define T13_MKDIR(p) _mkdir(p)
#define T13_CHDIR(p) _chdir(p)
#define T13_GETCWD(b, n) _getcwd(b, n)
#else
#define T13_MKDIR(p) mkdir(p, 0700)
#define T13_CHDIR(p) chdir(p)
#define T13_GETCWD(b, n) getcwd(b, n)
#endif

#if defined(_WIN32)
/* Replace an existing path with a new file. The test's source handle is
 * opened with fopen(), which does not share delete access, so the OS may
 * still refuse this; the caller treats that as an explicit platform skip. */
#define T13_REPLACE_FILE(from, to) MoveFileExA((from), (to), MOVEFILE_REPLACE_EXISTING)
#else
#define T13_REPLACE_FILE(from, to) rename((from), (to))
#endif

#if defined(_WIN32)
/* Returns 0 on success, -1 when the platform refuses symlinks, 1 on a
 * real error. */
static int t13_make_symlink(const char *target, const char *link) {
    if (CreateSymbolicLinkA(link, target, 0)) return 0;
    return GetLastError() == ERROR_PRIVILEGE_NOT_HELD ? -1 : 1;
}
#else
static int t13_make_symlink(const char *target, const char *link) {
    return symlink(target, link) == 0 ? 0 : -1;
}
#endif

enum { T13_DOUBLES = 360, T13_BYTES = T13_DOUBLES * 8 };

/* Minimal one-record mock whose data slot 2 carries a marker value, so
 * identity tests distinguish the original file (11) from a same-named
 * replacement (99) via jpl_read_record. */
static int t13_write_probe(const char *path, double marker) {
    const double constants[2] = {1.0, 2.0};
    jpl_fixture_series series;
    jpl_fixture_spec spec;
    double *coefficients = (double *)malloc(2 * sizeof(double));
    int rc;
    if (!coefficients) return 1;
    coefficients[0] = marker;
    coefficients[1] = 0.0;
    memset(&series, 0, sizeof series);
    series.slot = 11; /* nutations; coefficient 0 is the record marker */
    series.components = 2;
    series.coefficients_per_component = 1;
    series.subintervals = 1;
    series.coefficients = coefficients;
    memset(&spec, 0, sizeof spec);
    spec.start[0] = 2451545.0;
    spec.end[0] = 2451546.0;
    spec.step_days = 1.0;
    spec.denum = 441;
    spec.astronomical_unit_km = JPL_TEST_AU_KM;
    spec.earth_moon_mass_ratio = JPL_TEST_MASS_RATIO;
    spec.record_doubles = T13_DOUBLES;
    spec.records = 1;
    spec.constants = constants;
    spec.constant_count = 2;
    spec.series = &series;
    spec.series_count = 1;
    spec.title = "t13 probe";
    rc = jpl_fixture_write(path, &spec);
    free(coefficients);
    return rc;
}

static int t13_read_marker(jpl_ephemeris *e, double *marker) {
    static double vals[T13_DOUBLES];
    if (jpl_read_record(e, 0, vals, T13_DOUBLES) != JPL_OK) return 1;
    *marker = vals[2];
    return 0;
}

/* Cwd regression: a/probe.eph holds 11, b/probe.eph holds 99. Open
 * the relative path from a, move to b, clone, and require 11 from both
 * handles. A clone that reopens the caller's relative path would read 99.
 * Runs in a dedicated runtime sandbox and restores cwd on every exit;
 * never changes the agent's shell directory (only this process). */
static int t13_cwd_clone(void) {
    char orig[4096];
    jpl_ephemeris *e = NULL, *c = NULL;
    double me = 0.0, mc = 0.0;
    int rc = 0, in_sandbox = 0;
    if (T13_GETCWD(orig, sizeof orig) == NULL) { fprintf(stderr, "t13 cwd: getcwd failed\n"); return 1; }
    /* Dedicated sandbox under the CTest working directory. */
    T13_MKDIR("jpl_t13_work");
    T13_MKDIR("jpl_t13_work/a");
    T13_MKDIR("jpl_t13_work/b");
    if (t13_write_probe("jpl_t13_work/a/probe.eph", 11.0) != 0 ||
        t13_write_probe("jpl_t13_work/b/probe.eph", 99.0) != 0) {
        fprintf(stderr, "t13 cwd: probe write failed\n"); rc = 1; goto done;
    }
    if (T13_CHDIR("jpl_t13_work/a") != 0) { fprintf(stderr, "t13 cwd: chdir a failed\n"); rc = 1; goto done; }
    in_sandbox = 1;
    if (jpl_open("probe.eph", &e) != JPL_OK || e == NULL) {
        fprintf(stderr, "t13 cwd: open from a failed\n"); rc = 1; goto done;
    }
    /* Move to b while the handle stays open; the clone must still read a. */
    if (T13_CHDIR("../b") != 0) { fprintf(stderr, "t13 cwd: chdir b failed\n"); rc = 1; goto done; }
    if (jpl_ephemeris_clone(e, &c) != JPL_OK || c == NULL) {
        fprintf(stderr, "t13 cwd: clone from b failed\n"); rc = 1; goto done;
    }
    if (t13_read_marker(e, &me) != 0 || t13_read_marker(c, &mc) != 0) {
        fprintf(stderr, "t13 cwd: record read failed\n"); rc = 1; goto done;
    }
    if (me != 11.0 || mc != 11.0) {
        fprintf(stderr, "t13 cwd: identity lost (source=%g clone=%g, want 11/11)\n", me, mc);
        rc = 1; goto done;
    }
done:
    if (c) jpl_close(c);
    if (e) jpl_close(e);
    if (in_sandbox) {
        if (T13_CHDIR(orig) != 0) { fprintf(stderr, "t13 cwd: cwd restore failed\n"); rc = 1; }
    }
    remove("jpl_t13_work/a/probe.eph");
    remove("jpl_t13_work/b/probe.eph");
#ifdef _WIN32
    _rmdir("jpl_t13_work/a");
    _rmdir("jpl_t13_work/b");
    _rmdir("jpl_t13_work");
#else
    rmdir("jpl_t13_work/a");
    rmdir("jpl_t13_work/b");
    rmdir("jpl_t13_work");
#endif
    return rc;
}

/* Path-replacement test: after opening marker-11, replace the path
 * with marker-99 content. The clone must either keep the original
 * identity (reads 11) or fail cleanly with IO + NULL output; it must
 * never surface replacement data (99). A platform that refuses to
 * rename over the open file is reported as prohibited, not as a pass
 * of the replacement check itself. */
static int t13_replacement(void) {
    const char *orig = "jpl_t13_repl.eph";
    const char *fresh = "jpl_t13_repl_new.eph";
    jpl_ephemeris *e = NULL, *c = (jpl_ephemeris *)(uintptr_t)0x1;
    jpl_status st;
    double m = 0.0;
    int rc = 0;
    if (t13_write_probe(orig, 11.0) != 0 || t13_write_probe(fresh, 99.0) != 0) {
        fprintf(stderr, "t13 replacement: probe write failed\n"); rc = 1; goto done;
    }
    if (jpl_open(orig, &e) != JPL_OK || e == NULL) {
        fprintf(stderr, "t13 replacement: open failed\n"); rc = 1; goto done;
    }
#if defined(_WIN32)
    if (T13_REPLACE_FILE(fresh, orig) != 0) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
            /* The source handle does not share delete access. */
            fputs("t13 replacement: replacing an open file is prohibited on this platform (cwd test retains coverage)\n", stderr);
            goto done;
        }
        fprintf(stderr, "t13 replacement: replace failed (%lu)\n", (unsigned long)err);
        rc = 1;
        goto done;
    }
#else
    if (T13_REPLACE_FILE(fresh, orig) != 0) {
        fprintf(stderr, "t13 replacement: rename failed\n");
        rc = 1;
        goto done;
    }
#endif
    c = NULL;
    st = jpl_ephemeris_clone(e, &c);
    if (st == JPL_OK) {
        if (c == NULL) { fprintf(stderr, "t13 replacement: OK with NULL clone\n"); rc = 1; goto done; }
        if (t13_read_marker(c, &m) != 0) { fprintf(stderr, "t13 replacement: survivor read failed\n"); rc = 1; goto done; }
        if (m == 99.0) { fprintf(stderr, "t13 replacement: clone surfaced replacement data\n"); rc = 1; goto done; }
        if (m != 11.0) { fprintf(stderr, "t13 replacement: clone read %g, want 11\n", m); rc = 1; goto done; }
    } else if (st == JPL_ERROR_IO) {
        if (c != NULL) { fprintf(stderr, "t13 replacement: IO without NULL output\n"); rc = 1; goto done; }
    } else {
        fprintf(stderr, "t13 replacement: clone status %d, want OK or IO\n", (int)st);
        rc = 1; goto done;
    }
done:
    if (c && c != (jpl_ephemeris *)(uintptr_t)0x1) jpl_close(c);
    if (e) jpl_close(e);
    remove(orig);
    remove(fresh);
    return rc;
}

/* Symlink-retarget test (POSIX; Windows skips when
 * the OS disallows symlinks). Same oracle as replacement: original
 * identity or clean IO, never replacement data. */
static int t13_symlink(void) {
    const char *r1 = "jpl_t13_link1.eph";
    const char *r2 = "jpl_t13_link2.eph";
    const char *ln = "jpl_t13_link.eph";
    jpl_ephemeris *e = NULL, *c = NULL;
    jpl_status st;
    double m = 0.0;
    int rc = 0;
    int link_status;
    if (t13_write_probe(r1, 11.0) != 0 || t13_write_probe(r2, 99.0) != 0) {
        fprintf(stderr, "t13 symlink: probe write failed\n"); rc = 1; goto done;
    }
    remove(ln);
    link_status = t13_make_symlink(r1, ln);
    if (link_status != 0) {
        fprintf(stderr, "t13 symlink: creating a symlink is prohibited here; cwd test retains coverage\n");
        if (link_status > 0) rc = 1;
        goto done;
    }
    if (jpl_open(ln, &e) != JPL_OK || e == NULL) {
        fprintf(stderr, "t13 symlink: open failed\n"); rc = 1; goto done;
    }
    if (remove(ln) != 0 || t13_make_symlink(r2, ln) != 0) {
        fprintf(stderr, "t13 symlink: retarget prohibited here; cwd test retains coverage\n");
        goto done;
    }
    st = jpl_ephemeris_clone(e, &c);
    if (st == JPL_OK) {
        if (c == NULL) { fprintf(stderr, "t13 symlink: OK with NULL clone\n"); rc = 1; goto done; }
        if (t13_read_marker(c, &m) != 0) { fprintf(stderr, "t13 symlink: survivor read failed\n"); rc = 1; goto done; }
        if (m == 99.0) { fprintf(stderr, "t13 symlink: clone surfaced retargeted data\n"); rc = 1; goto done; }
        if (m != 11.0) { fprintf(stderr, "t13 symlink: clone read %g, want 11\n", m); rc = 1; goto done; }
    } else if (st == JPL_ERROR_IO) {
        if (c != NULL) { fprintf(stderr, "t13 symlink: IO without NULL output\n"); rc = 1; goto done; }
    } else {
        fprintf(stderr, "t13 symlink: clone status %d, want OK or IO\n", (int)st);
        rc = 1; goto done;
    }
done:
    if (c) jpl_close(c);
    if (e) jpl_close(e);
    remove(ln);
    remove(r1);
    remove(r2);
    return rc;
}

/* Cross-backend composition: the same file opened FILE and mmap must
 * serve identical bytes and states, and a RANGE query between valid
 * reads must disturb neither backend's cache. This pins the unified
 * read path (one validator, one reader behind the byte-source seam). */
static int t13_cross_backend(void) {
    const char *path = "jpl_t13_cross.eph";
    jpl_ephemeris *f = NULL, *m = NULL;
    static const jpl_date mid = {2451545.5, 0.0};
    static const jpl_date outside = {2451547.0, 0.0};
    static double fvals[T13_DOUBLES], mvals[T13_DOUBLES];
    jpl_state fs0, ms0, fs1, ms1;
    int rc = 0;
    if (t13_write_probe(path, 11.0) != 0) { fprintf(stderr, "t13 cross: probe write failed\n"); return 1; }
    if (jpl_open(path, &f) != JPL_OK || f == NULL) { fprintf(stderr, "t13 cross: file open failed\n"); rc = 1; goto done; }
    if (jpl_open_mmap(path, &m) != JPL_OK || m == NULL) { fprintf(stderr, "t13 cross: mmap open failed\n"); rc = 1; goto done; }
    /* Identical raw bytes through both readers. */
    if (jpl_read_record(f, 0, fvals, T13_DOUBLES) != JPL_OK ||
        jpl_read_record(m, 0, mvals, T13_DOUBLES) != JPL_OK) {
        fprintf(stderr, "t13 cross: record read failed\n"); rc = 1; goto done;
    }
    if (memcmp(fvals, mvals, sizeof fvals) != 0) { fprintf(stderr, "t13 cross: backend bytes differ\n"); rc = 1; goto done; }
    /* Identical states through the evaluator on both backends. */
    if (jpl_get_state(f, &mid, JPL_NUTATIONS, 0, &fs0) != JPL_OK ||
        jpl_get_state(m, &mid, JPL_NUTATIONS, 0, &ms0) != JPL_OK) {
        fprintf(stderr, "t13 cross: state read failed\n"); rc = 1; goto done;
    }
    if (memcmp(&fs0, &ms0, sizeof fs0) != 0) { fprintf(stderr, "t13 cross: backend states differ\n"); rc = 1; goto done; }
    /* A RANGE query between valid reads disturbs nothing on either side. */
    if (jpl_get_state(f, &outside, JPL_NUTATIONS, 0, &fs1) != JPL_ERROR_RANGE ||
        jpl_get_state(m, &outside, JPL_NUTATIONS, 0, &ms1) != JPL_ERROR_RANGE) {
        fprintf(stderr, "t13 cross: out-of-coverage not RANGE\n"); rc = 1; goto done;
    }
    if (jpl_get_state(f, &mid, JPL_NUTATIONS, 0, &fs1) != JPL_OK ||
        jpl_get_state(m, &mid, JPL_NUTATIONS, 0, &ms1) != JPL_OK) {
        fprintf(stderr, "t13 cross: reread after RANGE failed\n"); rc = 1; goto done;
    }
    if (memcmp(&fs1, &fs0, sizeof fs1) != 0 || memcmp(&ms1, &ms0, sizeof ms1) != 0) {
        fprintf(stderr, "t13 cross: reread differs (cache poisoned)\n"); rc = 1; goto done;
    }
done:
    if (f) jpl_close(f);
    if (m) jpl_close(m);
    remove(path);
    return rc;
}

/* Close-order lifetimes for FILE and mmap: close the original
 * before the clone and require valid survivor headers + record reads.
 * Sibling order is extended in test_api_errors.c. */
static int t13_lifetimes(void) {
    const char *fp = "jpl_t13_life_file.eph";
    const char *mp = "jpl_t13_life_mmap.eph";
    jpl_ephemeris *e = NULL, *c = NULL, *m = NULL, *mc = NULL;
    const jpl_header *h;
    double mk = 0.0;
    int rc = 0;
    if (t13_write_probe(fp, 11.0) != 0 || t13_write_probe(mp, 11.0) != 0) {
        fprintf(stderr, "t13 lifetimes: probe write failed\n"); return 1;
    }
    /* FILE: original closes first; survivor header + record stay valid. */
    if (jpl_open(fp, &e) != JPL_OK) { fprintf(stderr, "t13 lifetimes: file open failed\n"); rc = 1; goto done; }
    if (jpl_ephemeris_clone(e, &c) != JPL_OK || c == NULL) {
        fprintf(stderr, "t13 lifetimes: file clone failed\n"); rc = 1; goto done;
    }
    jpl_close(e); e = NULL;
    h = jpl_get_header(c);
    if (h == NULL || h->constant_count != 2 || jpl_record_count(c) != 1) {
        fprintf(stderr, "t13 lifetimes: file survivor header invalid\n"); rc = 1; goto done;
    }
    if (t13_read_marker(c, &mk) != 0 || mk != 11.0) {
        fprintf(stderr, "t13 lifetimes: file survivor read %g, want 11\n", mk); rc = 1; goto done;
    }
    /* mmap: same original-before-clone close order. */
    if (jpl_open_mmap(mp, &m) != JPL_OK) { fprintf(stderr, "t13 lifetimes: mmap open failed\n"); rc = 1; goto done; }
    if (jpl_ephemeris_clone(m, &mc) != JPL_OK || mc == NULL) {
        fprintf(stderr, "t13 lifetimes: mmap clone failed\n"); rc = 1; goto done;
    }
    jpl_close(m); m = NULL;
    h = jpl_get_header(mc);
    if (h == NULL || h->constant_count != 2 || jpl_record_count(mc) != 1) {
        fprintf(stderr, "t13 lifetimes: mmap survivor header invalid\n"); rc = 1; goto done;
    }
    if (t13_read_marker(mc, &mk) != 0 || mk != 11.0) {
        fprintf(stderr, "t13 lifetimes: mmap survivor read %g, want 11\n", mk); rc = 1; goto done;
    }
done:
    if (c) jpl_close(c);
    if (e) jpl_close(e);
    if (mc) jpl_close(mc);
    if (m) jpl_close(m);
    remove(fp);
    remove(mp);
    return rc;
}

int main(void){
    const char *path="jpl_test_mmap.bin";
    unsigned char rec[JPL_TEST_HEADER_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL,*c=NULL;
    const jpl_header *h; double vals[360]; size_t i;
    memcpy(rec,"mmap test",9);
    test_mock_header(rec,2451545.0,2451546.0,1.0,2);
    test_put_ptr(rec,11,2,178,1);
    test_put_ptr(rec,12,441,0,0);
    f=fopen(path,"wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
    { unsigned char mc[JPL_TEST_HEADER_BYTES]={0}; test_put_f64(mc,1.0); test_put_f64(mc+8,2.0);
      JPL_TEST_CHECK(fwrite(mc,1,sizeof mc,f)==sizeof mc); }
    memset(rec,0,sizeof rec);
    test_put_f64(rec+0*8,2451545.0); test_put_f64(rec+1*8,2451546.0);
    for(i=2;i<360;++i) test_put_f64(rec+i*8,(double)i);
    JPL_TEST_CHECK(fwrite(rec,1,360*8,f)==360*8); JPL_TEST_CHECK(fclose(f)==0);
    JPL_TEST_CHECK(jpl_open_mmap(path,&e)==JPL_OK);
    h=jpl_get_header(e); JPL_TEST_CHECK(h && h->constant_count==2 && jpl_record_count(e)==1);
    JPL_TEST_CHECK(jpl_ephemeris_clone(e,&c)==JPL_OK);
    JPL_TEST_CHECK(jpl_read_record(e,0,vals,360)==JPL_OK && vals[0]==2451545.0 && vals[356]==356.0);
    JPL_TEST_CHECK(jpl_read_record(c,0,vals,360)==JPL_OK && vals[1]==2451546.0);
    /* Sequentially interleaved state reads on an mmap handle match a clone;
     * concurrent reads are exercised separately in test_threads.c.
     * Uses a dedicated mock whose record bounds match header
     * coverage (the main mock above stores 0..356 as data, so its bounds
     * 0..1 cannot satisfy the jd window). */
    {
        const char *path2 = "jpl_test_mmap_share.bin";
        unsigned char rec2[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f2;
        jpl_ephemeris *e2 = NULL, *c2 = NULL;
        jpl_state a, b;
        int same = 1;
        memcpy(rec2, "mmap share", 10);
        test_mock_header(rec2, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec2, 11, 2, 178, 1);
        test_put_ptr(rec2, 12, 441, 0, 0);
        f2 = fopen(path2, "wb"); JPL_TEST_CHECK(f2);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        { unsigned char mc2[JPL_TEST_HEADER_BYTES]={0}; test_put_f64(mc2,1.0); test_put_f64(mc2+8,2.0);
          JPL_TEST_CHECK(fwrite(mc2,1,sizeof mc2,f2)==sizeof mc2); }
        memset(rec2, 0, sizeof rec2);
        test_put_f64(rec2 + 0 * 8, 2451545.0);
        test_put_f64(rec2 + 1 * 8, 2451546.0);
        JPL_TEST_CHECK(fwrite(rec2, 1, 360*8, f2) == 360*8);
        JPL_TEST_CHECK(fclose(f2) == 0);
        JPL_TEST_CHECK(jpl_open_mmap(path2, &e2) == JPL_OK);
        JPL_TEST_CHECK(jpl_ephemeris_clone(e2, &c2) == JPL_OK);
        { static const jpl_date jd = {2451545.5, 0.0};
          JPL_TEST_CHECK(jpl_get_state(e2, &jd, JPL_NUTATIONS, 0, &a) == JPL_OK);
          JPL_TEST_CHECK(jpl_get_state(c2, &jd, JPL_NUTATIONS, 0, &b) == JPL_OK);
          JPL_TEST_CHECK(jpl_get_state(e2, &jd, JPL_NUTATIONS, 0, &a) == JPL_OK); }
        if (a.x != b.x || a.y != b.y || a.z != b.z ||
            a.vx != b.vx || a.vy != b.vy || a.vz != b.vz) same = 0;
        JPL_TEST_CHECK(same);
        jpl_close(c2); jpl_close(e2);
        JPL_TEST_CHECK(remove(path2) == 0);
    }
    JPL_TEST_CHECK(jpl_ephemeris_clone(NULL,NULL)==JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_open_mmap(NULL,NULL)==JPL_ERROR_ARGUMENT);
    jpl_close(c); jpl_close(e);
    { jpl_ephemeris *fe=NULL, *fc=NULL;
      JPL_TEST_CHECK(jpl_open(path,&fe)==JPL_OK);
      JPL_TEST_CHECK(jpl_ephemeris_clone(fe,&fc)==JPL_OK);
      JPL_TEST_CHECK(jpl_read_record(fe,0,vals,360)==JPL_OK && vals[0]==2451545.0 && vals[356]==356.0);
      JPL_TEST_CHECK(jpl_read_record(fc,0,vals,360)==JPL_OK && vals[1]==2451546.0);
      jpl_close(fc); jpl_close(fe); }
    JPL_TEST_CHECK(remove(path)==0);
    if (t13_cross_backend() != 0) return 1;
    if (t13_cwd_clone() != 0) return 1;
    if (t13_replacement() != 0) return 1;
    if (t13_symlink() != 0) return 1;
    if (t13_lifetimes() != 0) return 1;
    puts("mmap tests passed"); return 0;
}
