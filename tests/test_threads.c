/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Concurrent-reader tests: shared mmap reads and FILE-clone lifetimes.
 * Mock: 8 one-day records over 2451545..2451553, linear Mars x(t),
 * constant-per-record nutations and TT-TDB, all analytically checkable
 * without calling the library as an oracle. */
#include "jpl.h"
#include "jpl_fixture.h"
#include "jpl_test_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

enum { THR_DOUBLES = 360, THR_RECORDS = 8 };
enum { THR_WORKERS = 4, THR_ITERS = 1000, THR_CHURN_ITERS = 100 };

static const double THR_POS_TOL = 1e-13;
static const double THR_VEL_TOL = 1e-14;
static const double THR_SMALL_TOL = 1e-15;

static const double THR_START = 2451545.0;

static int write_threads_mock(const char *path) {
    const double constants[2] = {1.0, 2.0};
    double mars[(size_t)THR_RECORDS * 3 * 2];
    double nutations[(size_t)THR_RECORDS * 2 * 2];
    double tt_tdb[(size_t)THR_RECORDS * 2];
    jpl_fixture_series series[2];
    jpl_fixture_spec spec;
    size_t r;

    for (r = 0; r < THR_RECORDS; ++r) {
        size_t mo = r * 3 * 2;
        size_t no = r * 2 * 2;
        size_t to = r * 2;
        mars[mo + 0] = (1.0 + 0.01 * ((double)r + 0.5)) * JPL_TEST_AU_KM;
        mars[mo + 1] = 0.005 * JPL_TEST_AU_KM;
        mars[mo + 2] = 0.0;
        mars[mo + 3] = 0.0;
        mars[mo + 4] = 0.0;
        mars[mo + 5] = 0.0;
        nutations[no + 0] = 0.01 + 0.001 * (double)r;
        nutations[no + 1] = 0.0;
        nutations[no + 2] = 0.02 + 0.001 * (double)r;
        nutations[no + 3] = 0.0;
        tt_tdb[to + 0] = 0.001 + 0.0001 * (double)r;
        tt_tdb[to + 1] = 0.0;
    }

    memset(series, 0, sizeof series);
    series[0].slot = 3; /* Mars */
    series[0].components = 3;
    series[0].coefficients_per_component = 2;
    series[0].subintervals = 1;
    series[0].coefficients = mars;
    series[1].slot = 11; /* nutations */
    series[1].components = 2;
    series[1].coefficients_per_component = 2;
    series[1].subintervals = 1;
    series[1].coefficients = nutations;

    memset(&spec, 0, sizeof spec);
    spec.start[0] = THR_START;
    spec.end[0] = THR_START + 8.0;
    spec.step_days = 1.0;
    spec.denum = 441;
    spec.astronomical_unit_km = JPL_TEST_AU_KM;
    spec.earth_moon_mass_ratio = JPL_TEST_MASS_RATIO;
    spec.record_doubles = THR_DOUBLES;
    spec.records = THR_RECORDS;
    spec.constants = constants;
    spec.constant_count = 2;
    spec.series = series;
    spec.series_count = 2;
    spec.tt_tdb_present = 1;
    spec.tt_tdb_coeffs = 2;
    spec.tt_tdb_subintervals = 1;
    spec.tt_tdb_coefficients = tt_tdb;
    spec.title = "threads mock";
    return jpl_fixture_write(path, &spec);
}

typedef struct worker_args {
    jpl_ephemeris *ephemeris;
    int worker_index, failed;
} worker_args;

static void run_queries(worker_args *a) {
    for (int k = 0; k < THR_ITERS; ++k) {
        jpl_date jd = {2451545.0 + (double)((k + a->worker_index) % 8), 0.25};
        jpl_state state;
        if (jpl_get_state(a->ephemeris, &jd, JPL_MARS, 0, &state) != JPL_OK) {
            a->failed = 1;
            return;
        }
        /* For this mock x(t) = 1 + 0.01*(t-2451545), in modern AU. */
        double expected = 1.0 + 0.01*((jd.hi-2451545.0)+jd.lo);
        if (fabs(state.x-expected) > THR_POS_TOL || fabs(state.vx-0.01) > THR_VEL_TOL) {
            a->failed = 1;
            return;
        }
    }
}

static void run_tttdb_queries(worker_args *a) {
    for (int k = 0; k < THR_ITERS; ++k) {
        jpl_date jd = {2451545.0 + (double)((k + a->worker_index) % 8), 0.25};
        double sec = 0.0, rate = -1.0;
        double rec = (double)((k + a->worker_index) % 8);
        double expected = 0.001 + 0.0001 * rec;
        if (jpl_get_tt_minus_tdb(a->ephemeris, &jd, &sec, &rate) != JPL_OK) {
            a->failed = 1;
            return;
        }
        if (fabs(sec - expected) > THR_SMALL_TOL || fabs(rate) > THR_SMALL_TOL) {
            a->failed = 1;
            return;
        }
    }
}

static void run_angular_queries(worker_args *a) {
    for (int k = 0; k < THR_ITERS; ++k) {
        jpl_date jd = {2451545.0 + (double)((k + a->worker_index) % 8), 0.25};
        jpl_state state;
        double rec = (double)((k + a->worker_index) % 8);
        double want_dpsi = 0.01 + 0.001 * rec;
        double want_deps = 0.02 + 0.001 * rec;
        if (jpl_get_state(a->ephemeris, &jd, JPL_NUTATIONS, 0, &state) != JPL_OK) {
            a->failed = 1;
            return;
        }
        if (fabs(state.x - want_dpsi) > THR_SMALL_TOL || fabs(state.y - want_deps) > THR_SMALL_TOL ||
            fabs(state.vx) > THR_SMALL_TOL || fabs(state.vy) > THR_SMALL_TOL ||
            fabs(state.z) > THR_SMALL_TOL || fabs(state.vz) > THR_SMALL_TOL) {
            a->failed = 1;
            return;
        }
    }
}

static void run_full(worker_args *a) {
    run_queries(a);
    if (a->failed) return;
    run_tttdb_queries(a);
    if (a->failed) return;
    run_angular_queries(a);
}

#if defined(_WIN32)
static HANDLE thr_gate = NULL;
static DWORD WINAPI thr_entry(LPVOID param) {
    worker_args *a = (worker_args *)param;
    WaitForSingleObject(thr_gate, INFINITE);
    run_full(a);
    return 0;
}
static int launch_and_join(worker_args *args, int n) {
    HANDLE handles[THR_WORKERS];
    int i;
    thr_gate = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (thr_gate == NULL) return 1;
    for (i = 0; i < n; ++i) {
        handles[i] = CreateThread(NULL, 0, thr_entry, &args[i], 0, NULL);
        if (handles[i] == NULL) {
            SetEvent(thr_gate);
            for (--i; i >= 0; --i) WaitForSingleObject(handles[i], INFINITE);
            CloseHandle(thr_gate);
            thr_gate = NULL;
            return 1;
        }
    }
    SetEvent(thr_gate);
    for (i = 0; i < n; ++i) {
        WaitForSingleObject(handles[i], INFINITE);
        CloseHandle(handles[i]);
    }
    CloseHandle(thr_gate);
    thr_gate = NULL;
    for (i = 0; i < n; ++i) {
        if (args[i].failed) return 1;
    }
    return 0;
}
#else
static pthread_mutex_t thr_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thr_cv = PTHREAD_COND_INITIALIZER;
static int thr_go = 0;
static void *thr_entry(void *param) {
    worker_args *a = (worker_args *)param;
    (void)pthread_mutex_lock(&thr_mu);
    while (!thr_go) (void)pthread_cond_wait(&thr_cv, &thr_mu);
    (void)pthread_mutex_unlock(&thr_mu);
    run_full(a);
    return NULL;
}
static int launch_and_join(worker_args *args, int n) {
    pthread_t threads[THR_WORKERS];
    int i, rc;
    thr_go = 0;
    for (i = 0; i < n; ++i) {
        rc = pthread_create(&threads[i], NULL, thr_entry, &args[i]);
        if (rc != 0) {
            (void)pthread_mutex_lock(&thr_mu);
            thr_go = 1;
            (void)pthread_cond_broadcast(&thr_cv);
            (void)pthread_mutex_unlock(&thr_mu);
            for (--i; i >= 0; --i) (void)pthread_join(threads[i], NULL);
            return 1;
        }
    }
    (void)pthread_mutex_lock(&thr_mu);
    thr_go = 1;
    (void)pthread_cond_broadcast(&thr_cv);
    (void)pthread_mutex_unlock(&thr_mu);
    for (i = 0; i < n; ++i) (void)pthread_join(threads[i], NULL);
    for (i = 0; i < n; ++i) {
        if (args[i].failed) return 1;
    }
    return 0;
}
#endif

#if !defined(_WIN32)
/* Churn readers reuse the gated POSIX entry point (thr_entry). */
#endif

static int phase_shared_mmap(const char *path) {
    jpl_ephemeris *e = NULL;
    worker_args args[THR_WORKERS];
    int i;
    if (jpl_open_mmap(path, &e) != JPL_OK || e == NULL) {
        fprintf(stderr, "threads: mmap open failed\n");
        return 1;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        args[i].ephemeris = e;
        args[i].worker_index = i;
        args[i].failed = 0;
    }
    if (launch_and_join(args, THR_WORKERS) != 0) {
        fprintf(stderr, "threads: shared mmap workers failed\n");
        jpl_close(e);
        return 1;
    }
    jpl_close(e);
    return 0;
}

static int phase_file_clones(const char *path) {
    jpl_ephemeris *src = NULL;
    jpl_ephemeris *clones[THR_WORKERS] = {NULL, NULL, NULL, NULL};
    worker_args args[THR_WORKERS];
    int i, rc = 0;
    if (jpl_open(path, &src) != JPL_OK || src == NULL) {
        fprintf(stderr, "threads: file open failed\n");
        return 1;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        if (jpl_ephemeris_clone(src, &clones[i]) != JPL_OK || clones[i] == NULL) {
            fprintf(stderr, "threads: file clone %d failed\n", i);
            rc = 1;
            goto done;
        }
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        args[i].ephemeris = clones[i];
        args[i].worker_index = i;
        args[i].failed = 0;
    }
    if (launch_and_join(args, THR_WORKERS) != 0) {
        fprintf(stderr, "threads: file clone workers failed\n");
        rc = 1;
        goto done;
    }
done:
    for (i = 0; i < THR_WORKERS; ++i) {
        if (clones[i]) jpl_close(clones[i]);
    }
    if (src) jpl_close(src);
    return rc;
}

/* Close the unused original before readers start; siblings close in
 * reverse order after their own joins. */
static int phase_orphan_clones(const char *path) {
    jpl_ephemeris *src = NULL;
    jpl_ephemeris *clones[THR_WORKERS] = {NULL, NULL, NULL, NULL};
    worker_args args[THR_WORKERS];
    int i, rc = 0;
    if (jpl_open(path, &src) != JPL_OK || src == NULL) {
        fprintf(stderr, "threads: orphan open failed\n");
        return 1;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        if (jpl_ephemeris_clone(src, &clones[i]) != JPL_OK || clones[i] == NULL) {
            fprintf(stderr, "threads: orphan clone %d failed\n", i);
            rc = 1;
            goto done;
        }
    }
    jpl_close(src);
    src = NULL;
    for (i = 0; i < THR_WORKERS; ++i) {
        args[i].ephemeris = clones[i];
        args[i].worker_index = i;
        args[i].failed = 0;
    }
    if (launch_and_join(args, THR_WORKERS) != 0) {
        fprintf(stderr, "threads: orphan clone workers failed\n");
        rc = 1;
        goto done;
    }
done:
    for (i = THR_WORKERS - 1; i >= 0; --i) {
        if (clones[i]) jpl_close(clones[i]);
    }
    if (src) jpl_close(src);
    return rc;
}

/* Source stays alive while other clones read; the parent performs
 * synchronized clone/close churn on throwaway handles only. */
static int phase_churn(const char *path) {
    jpl_ephemeris *src = NULL;
    jpl_ephemeris *readers[THR_WORKERS] = {NULL, NULL, NULL, NULL};
    worker_args args[THR_WORKERS];
    int i, rc = 0;
#if defined(_WIN32)
    HANDLE handles[THR_WORKERS] = {NULL, NULL, NULL, NULL};
    int created = 0;
    thr_gate = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (thr_gate == NULL) return 1;
    if (jpl_open(path, &src) != JPL_OK || src == NULL) {
        fprintf(stderr, "threads: churn open failed\n");
        CloseHandle(thr_gate);
        thr_gate = NULL;
        return 1;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        if (jpl_ephemeris_clone(src, &readers[i]) != JPL_OK || readers[i] == NULL) {
            fprintf(stderr, "threads: churn clone %d failed\n", i);
            rc = 1;
            goto done_win;
        }
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        args[i].ephemeris = readers[i];
        args[i].worker_index = i;
        args[i].failed = 0;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        handles[i] = CreateThread(NULL, 0, thr_entry, &args[i], 0, NULL);
        if (handles[i] == NULL) {
            fprintf(stderr, "threads: churn create %d failed\n", i);
            rc = 1;
            break;
        }
        ++created;
    }
    /* Signal the gate even on partial startup so started workers cannot
     * block forever on WaitForSingleObject while the parent joins. */
    SetEvent(thr_gate);
    for (i = 0; i < THR_CHURN_ITERS; ++i) {
        jpl_ephemeris *tmp = NULL;
        if (jpl_ephemeris_clone(src, &tmp) != JPL_OK || tmp == NULL) {
            fprintf(stderr, "threads: churn iteration %d failed\n", i);
            rc = 1;
            break;
        }
        jpl_close(tmp);
    }
    for (i = 0; i < created; ++i) {
        if (handles[i]) {
            WaitForSingleObject(handles[i], INFINITE);
            CloseHandle(handles[i]);
            handles[i] = NULL;
        }
    }
    /* Only joined workers wrote results; unstarted args keep failed == 0. */
    for (i = 0; i < created; ++i) {
        if (args[i].failed) {
            fprintf(stderr, "threads: churn worker %d failed\n", i);
            rc = 1;
        }
    }
done_win:
    CloseHandle(thr_gate);
    thr_gate = NULL;
    for (i = 0; i < THR_WORKERS; ++i) {
        if (handles[i]) {
            WaitForSingleObject(handles[i], INFINITE);
            CloseHandle(handles[i]);
        }
        if (readers[i]) jpl_close(readers[i]);
    }
    if (src) jpl_close(src);
    return rc;
#else
    pthread_t threads[THR_WORKERS];
    int created = 0;
    if (jpl_open(path, &src) != JPL_OK || src == NULL) {
        fprintf(stderr, "threads: churn open failed\n");
        return 1;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        if (jpl_ephemeris_clone(src, &readers[i]) != JPL_OK || readers[i] == NULL) {
            fprintf(stderr, "threads: churn clone %d failed\n", i);
            rc = 1;
            goto done_posix;
        }
    }
    thr_go = 0;
    for (i = 0; i < THR_WORKERS; ++i) {
        args[i].ephemeris = readers[i];
        args[i].worker_index = i;
        args[i].failed = 0;
    }
    for (i = 0; i < THR_WORKERS; ++i) {
        if (pthread_create(&threads[i], NULL, thr_entry, &args[i]) != 0) {
            fprintf(stderr, "threads: churn create %d failed\n", i);
            rc = 1;
            break;
        }
        ++created;
    }
    (void)pthread_mutex_lock(&thr_mu);
    thr_go = 1;
    (void)pthread_cond_broadcast(&thr_cv);
    (void)pthread_mutex_unlock(&thr_mu);
    for (i = 0; i < THR_CHURN_ITERS; ++i) {
        jpl_ephemeris *tmp = NULL;
        if (jpl_ephemeris_clone(src, &tmp) != JPL_OK || tmp == NULL) {
            fprintf(stderr, "threads: churn iteration %d failed\n", i);
            rc = 1;
            break;
        }
        jpl_close(tmp);
    }
    for (i = 0; i < created; ++i) (void)pthread_join(threads[i], NULL);
    /* Only joined workers wrote results; unstarted args keep failed == 0. */
    for (i = 0; i < created; ++i) {
        if (args[i].failed) {
            fprintf(stderr, "threads: churn worker %d failed\n", i);
            rc = 1;
        }
    }
done_posix:
    for (i = 0; i < THR_WORKERS; ++i) {
        if (readers[i]) jpl_close(readers[i]);
    }
    if (src) jpl_close(src);
    return rc;
#endif
}

int main(void) {
    const char *path = "jpl_test_threads.bin";
    if (write_threads_mock(path) != 0) {
        fprintf(stderr, "threads: mock write failed\n");
        return 1;
    }
    if (phase_shared_mmap(path) != 0) return 1;
    if (phase_file_clones(path) != 0) return 1;
    if (phase_orphan_clones(path) != 0) return 1;
    if (phase_churn(path) != 0) return 1;
    if (remove(path) != 0) {
        fprintf(stderr, "threads: cleanup failed\n");
        return 1;
    }
    puts("threads tests passed");
    return 0;
}
