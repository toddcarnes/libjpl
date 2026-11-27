/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_convenience.h"
#include "jpl_test_util.h"
#include "jpl_private.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "jpl_test_tttdb.bin";
    /* Absent dataset: a valid in-coverage query reports UNSUPPORTED;
     * out-of-coverage stays RANGE. */
    {
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        const jpl_header *h;
        double sec = 0.0, rate = 0.0;
        size_t i;
        memcpy(rec, "no TT-TDB here", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        for (i = 2; i < 357; ++i) test_put_f64(rec + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->has_tt_tdb == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(NULL) == 0);
        /* A valid in-coverage query for a missing dataset reports
         * UNSUPPORTED, while an out-of-coverage query stays RANGE. */
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5, 0.0}, &sec, &rate) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.0 - 10.0, 0.0}, &sec, &rate) == JPL_ERROR_RANGE);
        /* Transactional outputs: both optionals unchanged on failure. */
        sec = 9.0; rate = 9.0;
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5, 0.0}, &sec, &rate) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(sec == 9.0 && rate == 9.0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(NULL, &(const jpl_date){2451545.5, 0.0}, &sec, &rate) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5, 0.0}, NULL, &rate) == JPL_ERROR_ARGUMENT);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Present dataset, NULL rate allowed; out-of-coverage is RANGE. */
    {
        static const jpl_date t16_mid = {2451545.0, 0.5};
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        double sec = 0.0, rate = -1.0;
        memcpy(rec, "TT-TDB present", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        test_put_ptr(rec, 14, 300, 2, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 299 * 8, 0.001);
        test_put_f64(rec + 300 * 8, 0.00002);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) != 0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5, 0.0}, &sec, NULL) == JPL_OK);
        /* Exact independent value/rate for the first linear mock:
         * c0=0.001, c1=0.00002 per unit Chebyshev argument, so at the
         * midpoint (x=0) the value is c0 and the daily rate is
         * 2*c1/substep with substep=1 day. Finiteness alone is not the
         * oracle here. */
        {
            static const double mid[2] = {2451545.0, 0.5};
            (void)mid;
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &t16_mid, &sec, &rate) == JPL_OK);
            JPL_TEST_CHECK(fabs(sec - 0.001) < 1e-18);
            JPL_TEST_CHECK(fabs(rate - 0.00004) < 1e-19);
        }
        JPL_TEST_CHECK(isfinite(rate));
        /* Byte-swapped twin: identical logical header/record encoded
         * big-endian must open with the BE order detected and return
         * the same value/rate. */
        {
            enum { BDBL = 360 };
            static unsigned char bhdr[360 * 8];
            static double bcons[BDBL];
            static double bvals[BDBL];
            jpl_test_mock bm;
            jpl_ephemeris *be = NULL;
            double bsec = 0.0, brate = 0.0;
            size_t bi, k;
            memset(bhdr, 0, sizeof bhdr);
            memcpy(bhdr, "TT-TDB present", 14);
            test_mock_header_be(bhdr, 2451545.0, 2451546.0, 1.0, 2);
            test_put_ptr_be(bhdr, 11, 2, 178, 1);
            test_put_ptr_be(bhdr, 14, 300, 2, 1);
            /* test_write_mock re-encodes the prologue words (coverage,
             * step, AU, EMRAT, DENUM) in the requested byte order;
             * geometry pointers above are encoded BE directly. */
            for (bi = 0; bi < BDBL; ++bi) bcons[bi] = 0.0;
            bcons[0] = 1.0;
            bcons[1] = 2.0;
            for (k = 0; k < BDBL; ++k) bvals[k] = 0.0;
            bvals[0] = 2451545.0;
            bvals[1] = 2451546.0;
            bvals[299] = 0.001;
            bvals[300] = 0.00002;
            memset(&bm, 0, sizeof bm);
            bm.record_doubles = BDBL;
            bm.advertised_records = 1;
            bm.big_endian = 1;
            bm.denum = 441;
            bm.constant_count = 2;
            bm.start[0] = 2451545.0;
            bm.end[0] = 2451546.0;
            bm.step_days = 1.0;
            bm.au_km = JPL_TEST_AU_KM;
            bm.emrat = JPL_TEST_MASS_RATIO;
            bm.header = bhdr;
            bm.constants = bcons;
            bm.records = bvals;
            JPL_TEST_CHECK(test_write_mock("jpl_test_tttdb_be.bin", &bm) == 0);
            JPL_TEST_CHECK(jpl_open("jpl_test_tttdb_be.bin", &be) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_header(be)->byte_order == JPL_BYTE_ORDER_BIG_ENDIAN);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(be, &t16_mid, &bsec, &brate) == JPL_OK);
            JPL_TEST_CHECK(fabs(bsec - 0.001) < 1e-18);
            JPL_TEST_CHECK(fabs(brate - 0.00004) < 1e-19);
            jpl_close(be);
            JPL_TEST_CHECK(remove("jpl_test_tttdb_be.bin") == 0);
        }
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.0 - 10.0, 0.0}, &sec, &rate) == JPL_ERROR_RANGE);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* DE430t shape regression: TT-TDB start (939) past the classic
     * maximum (938 via libration 900 + 3*13*1) must read as present,
     * with the record grown past 938 and a finite read. Byte order is a
     * file property, not a separate mock encoding here: every block
     * below opens the same file through FILE and mmap, which share one
     * validator and one swap-aware read path (read_f64/order), so the
     * duplicated assertions pin both backends on identical bytes. */
    {
        enum { RDBL = 984, RBYTES = RDBL * 8 };
        static unsigned char hdr[RBYTES];
        static unsigned char dat[RBYTES];
        static unsigned char con[RBYTES];
        FILE *f;
        jpl_ephemeris *e = NULL, *m = NULL;
        const jpl_header *h;
        double sec = 0.0, rate = 0.0;
        memset(hdr, 0, sizeof hdr);
        memset(dat, 0, sizeof dat);
        memset(con, 0, sizeof con);
        memcpy(hdr, "TT-TDB past max", 15);
        test_mock_header(hdr, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(hdr, 11, 2, 178, 1);
        test_put_ptr(hdr, 12, 441, 900, 13);
        test_put_u32(hdr + 2852, 1);
        test_put_ptr(hdr, 14, 939, 11, 4);
        test_put_f64(con, 1.0); test_put_f64(con + 8, 2.0);
        test_put_f64(dat + 0 * 8, 2451545.0);
        test_put_f64(dat + 1 * 8, 2451546.0);
        /* Four subintervals, eleven coefficients each: subinterval s
         * has c0/c1 at slots 939+s*11 (1-based), i.e. doubles
         * 938+s*11. Distinct constant/linear values per subinterval. */
        test_put_f64(dat + 938 * 8, 0.001);
        test_put_f64(dat + 939 * 8, 0.00002);
        test_put_f64(dat + 949 * 8, 0.002);
        test_put_f64(dat + 950 * 8, 0.00003);
        test_put_f64(dat + 960 * 8, 0.003);
        test_put_f64(dat + 961 * 8, -0.00001);
        test_put_f64(dat + 971 * 8, 0.004);
        test_put_f64(dat + 972 * 8, 0.00005);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
        JPL_TEST_CHECK(fwrite(con, 1, sizeof con, f) == sizeof con);
        JPL_TEST_CHECK(fwrite(dat, 1, sizeof dat, f) == sizeof dat);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->has_tt_tdb == 1);
        JPL_TEST_CHECK(h->record_double_count > 938);
        JPL_TEST_CHECK(h->record_double_count == RDBL);
        JPL_TEST_CHECK(h->tt_tdb_pointer.start_index == 939);
        JPL_TEST_CHECK(h->tt_tdb_pointer.coefficients_per_component == 11);
        JPL_TEST_CHECK(h->tt_tdb_pointer.subintervals == 4);
        JPL_TEST_CHECK(jpl_get_header(e)->has_tt_tdb == 1);
        /* Chebyshev value/rate at each subinterval midpoint:
         * value = c0, rate = 2*c1/substep with substep = 0.25 day.
          * Low-part observability: the same midpoint reached via a
         * split pair ({hi,lo} with tiny lo) must agree, and each end of
         * every subinterval plus low-part offsets around each interior
         * boundary selects the expected neighbor. */
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.625, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.003) < 1e-15 && fabs(rate - 8.0 * -0.00001) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.125, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.001) < 1e-15 && fabs(rate - 8.0 * 0.00002) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.375, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.002) < 1e-15 && fabs(rate - 8.0 * 0.00003) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.875, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.004) < 1e-15 && fabs(rate - 8.0 * 0.00005) < 1e-12);
        /* Same file via mmap: FILE and mmap share one validator, so
         * width, constants, and all four subinterval midpoints must agree
         * on both handles. */
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_header(m)->record_double_count == RDBL);
        JPL_TEST_CHECK(jpl_get_header(m)->has_tt_tdb == 1);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.125, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.001) < 1e-15 && fabs(rate - 8.0 * 0.00002) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.375, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.002) < 1e-15 && fabs(rate - 8.0 * 0.00003) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.625, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.003) < 1e-15 && fabs(rate - 8.0 * -0.00001) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.875, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.004) < 1e-15 && fabs(rate - 8.0 * 0.00005) < 1e-12);
        /* Equivalent split of the first midpoint plus both ends of
         * every subinterval. Neighbor offsets around each interior
         * boundary assert the selected side by value: just below a
         * boundary the evaluation stays in the left subinterval near
         * x=+1 (value c0+c1), just above it enters the right one near
         * x=-1 (value c0-c1). The 1e-9-day offset exceeds the
         * ~4.7e-10-day ulp of 2451545, so both sides are representable;
         * 2^-40 day alone is not. The mmap handle duplicates the
          * representative split/boundary checks on identical bytes. The
          * separately encoded BE mock above checks foreign byte order. */
        {
            double s2 = 0.0, r2 = 0.0;
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.0, 0.125}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - 0.001) < 1e-15 && fabs(r2 - 8.0 * 0.00002) < 1e-12);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.0, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.001 - 0.00002)) < 1e-15);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451546.0, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.004 + 0.00005)) < 1e-15);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.25 - 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.001 + 0.00002)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.25 + 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.002 - 0.00003)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5 - 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.002 + 0.00003)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5 + 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.003 + 0.00001)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.75 - 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.003 - 0.00001)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.75 + 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.004 - 0.00005)) < 1e-9);
            /* Byte-order duplicate: the same checks on the mmap handle,
             * which shares the swap-aware read path (read_f64/order). */
            JPL_TEST_CHECK(jpl_get_header(m)->byte_order == jpl_get_header(e)->byte_order);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.0, 0.125}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - 0.001) < 1e-15 && fabs(r2 - 8.0 * 0.00002) < 1e-12);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.5 + 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.003 + 0.00001)) < 1e-9);
        }
        jpl_close(m);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Boundary-equality TT-TDB span: (start 358 - 1) + 3 coeff * 1 sub
     * equals the 360-double record width. The final
     * subinterval is fully populated, so an in-coverage query must
     * succeed (regression for the DE440t-class off-by-one where
     * span_end == width was rejected as absent). */
    {
        enum { BDBL = 360 };
        static unsigned char bhdr[BDBL * 8];
        static double bcon[BDBL];
        static double bdat[BDBL];
        jpl_test_mock bmock;
        FILE *bf;
        jpl_ephemeris *be = NULL, *bm = NULL;
        const jpl_header *bh;
        double bsec = 0.0, brate = 0.0;
        size_t bi;
        memset(bhdr, 0, sizeof bhdr);
        memcpy(bhdr, "TT-TDB boundary", 15);
        test_mock_header(bhdr, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(bhdr, 11, 2, 178, 1);
        test_put_ptr(bhdr, 12, 441, 0, 0);
        test_put_u32(bhdr + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET, 358u);
        test_put_u32(bhdr + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 4, 3u);
        test_put_u32(bhdr + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 8, 1u);
        for (bi = 0; bi < BDBL; ++bi) bcon[bi] = 0.0;
        bcon[0] = 1.0;
        bcon[1] = 2.0;
        for (bi = 0; bi < BDBL; ++bi) bdat[bi] = 0.0;
        bdat[0] = 2451545.0;
        bdat[1] = 2451546.0;
        bdat[357] = 0.009;
        bdat[358] = 0.0;
        bdat[359] = 0.0;
        memset(&bmock, 0, sizeof bmock);
        bmock.record_doubles = BDBL;
        bmock.advertised_records = 1;
        bmock.denum = 441;
        bmock.constant_count = 2;
        bmock.start[0] = 2451545.0;
        bmock.end[0] = 2451546.0;
        bmock.step_days = 1.0;
        bmock.au_km = JPL_TEST_AU_KM;
        bmock.emrat = JPL_TEST_MASS_RATIO;
        bmock.header = bhdr;
        bmock.constants = bcon;
        bmock.records = bdat;
        bf = fopen(path, "wb"); JPL_TEST_CHECK(bf != NULL);
        JPL_TEST_CHECK(test_write_mock(path, &bmock) == 0);
        JPL_TEST_CHECK(fclose(bf) == 0);
        JPL_TEST_CHECK(jpl_open(path, &be) == JPL_OK);
        bh = jpl_get_header(be);
        JPL_TEST_CHECK(bh != NULL && bh->has_tt_tdb == 1);
        JPL_TEST_CHECK(bh->record_double_count == BDBL);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(be, &(const jpl_date){2451545.5, 0.0}, &bsec, &brate) == JPL_OK);
        JPL_TEST_CHECK(fabs(bsec - 0.009) < 1e-18);
        JPL_TEST_CHECK(fabs(brate) < 1e-18);
        JPL_TEST_CHECK(jpl_open_mmap(path, &bm) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(bm, &(const jpl_date){2451545.5, 0.0}, &bsec, &brate) == JPL_OK);
        JPL_TEST_CHECK(fabs(bsec - 0.009) < 1e-18);
        JPL_TEST_CHECK(fabs(brate) < 1e-18);
        jpl_close(bm);
        jpl_close(be);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Overflow-names header: >400 constants places the T15 tail after
     * the overflow names; a valid appended TT-TDB triple there must
     * read as present with exact record geometry. */
    {
        enum { NCONST = 401, EXTRA = NCONST - 400 };
        enum { RDBL = 984, RBYTES = RDBL * 8 };
        static unsigned char hdr[RBYTES];
        static unsigned char dat[RBYTES];
        static double cons[RDBL];
        unsigned char cons_raw[RBYTES];
        size_t tail = 0;
        FILE *f;
        jpl_ephemeris *e = NULL, *m = NULL;
        const jpl_header *h;
        double sec = 0.0, rate = 0.0;
        size_t i;
        JPL_TEST_CHECK(jpl_tail_offset(EXTRA, &tail) == JPL_OK);
        memset(hdr, 0, sizeof hdr);
        memset(dat, 0, sizeof dat);
        memcpy(hdr, "TT-TDB overflow", 15);
        test_mock_header(hdr, 2451545.0, 2451546.0, 1.0, NCONST);
        test_put_ptr(hdr, 11, 2, 178, 1);
        test_put_ptr(hdr, 12, 441, 900, 13);
        test_put_u32(hdr + 2852, 1);
        /* T15 tail sits after the overflow names at TAIL; T14 carries
         * an irrelevant out-of-record triple that must not invalidate
         * the separately valid T15 dataset. */
        test_put_u32(hdr + tail + 0, 1u);
        test_put_u32(hdr + tail + 4, 1000000u);
        test_put_u32(hdr + tail + 8, 1000000u);
        test_put_u32(hdr + tail + 12, 939u);
        test_put_u32(hdr + tail + 16, 11u);
        test_put_u32(hdr + tail + 20, 4u);
        for (i = 0; i < RDBL; ++i) cons[i] = 0.0;
        cons[0] = 1.0;
        cons[1] = 2.0;
        for (i = 0; i < RBYTES; i += 8) {
            double v = cons[i / 8];
            test_put_f64(cons_raw + i, v);
        }
        test_put_f64(dat + 0 * 8, 2451545.0);
        test_put_f64(dat + 1 * 8, 2451546.0);
        /* All four subintervals get distinct c0/c1 so every midpoint is
         * an independent value/rate check, mirroring the DE430t block. */
        test_put_f64(dat + 938 * 8, 0.005);
        test_put_f64(dat + 939 * 8, 0.00004);
        test_put_f64(dat + 949 * 8, 0.006);
        test_put_f64(dat + 950 * 8, 0.00005);
        test_put_f64(dat + 960 * 8, 0.007);
        test_put_f64(dat + 961 * 8, 0.00006);
        test_put_f64(dat + 971 * 8, 0.008);
        test_put_f64(dat + 972 * 8, 0.00007);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
        JPL_TEST_CHECK(fwrite(cons_raw, 1, sizeof cons_raw, f) == sizeof cons_raw);
        JPL_TEST_CHECK(fwrite(dat, 1, sizeof dat, f) == sizeof dat);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->has_tt_tdb == 1);
        JPL_TEST_CHECK(h->constant_count == NCONST);
        JPL_TEST_CHECK(h->record_double_count == RDBL);
        JPL_TEST_CHECK(jpl_record_count(e) == 1);
        JPL_TEST_CHECK(jpl_read_constants(e, cons, RDBL) == JPL_OK);
        JPL_TEST_CHECK(cons[0] == 1.0 && cons[1] == 2.0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.625, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.007) < 1e-15 && fabs(rate - 8.0 * 0.00006) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.125, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.005) < 1e-15 && fabs(rate - 8.0 * 0.00004) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.375, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.006) < 1e-15 && fabs(rate - 8.0 * 0.00005) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.875, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.008) < 1e-15 && fabs(rate - 8.0 * 0.00007) < 1e-12);
        /* Same file via mmap: FILE and mmap share one validator, so
         * width, constants, and the mid-coverage value/rate agree. */
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_header(m)->record_double_count == RDBL);
        JPL_TEST_CHECK(jpl_get_header(m)->constant_count == NCONST);
        JPL_TEST_CHECK(jpl_get_header(m)->has_tt_tdb == 1);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.125, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.005) < 1e-15 && fabs(rate - 8.0 * 0.00004) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.375, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.006) < 1e-15 && fabs(rate - 8.0 * 0.00005) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.625, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.007) < 1e-15 && fabs(rate - 8.0 * 0.00006) < 1e-12);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.875, 0.0}, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(fabs(sec - 0.008) < 1e-15 && fabs(rate - 8.0 * 0.00007) < 1e-12);
        /* Representative split/endpoint/low-part cases on the
         * overflow-names layout, both backends. */
        {
            double s2 = 0.0, r2 = 0.0;
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.0, 0.125}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - 0.005) < 1e-15 && fabs(r2 - 8.0 * 0.00004) < 1e-12);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, &(const jpl_date){2451545.5 + 1e-9, 0.0}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - (0.007 - 0.00006)) < 1e-9);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451545.0, 0.125}, &s2, &r2) == JPL_OK);
            JPL_TEST_CHECK(fabs(s2 - 0.005) < 1e-15 && fabs(r2 - 8.0 * 0.00004) < 1e-12);
            JPL_TEST_CHECK(jpl_get_tt_minus_tdb(m, &(const jpl_date){2451546.0, 0.0}, &s2, &r2) == JPL_OK);
        }
        jpl_close(m);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* TT→TDB policy: file refinement over a guaranteed model baseline.
     * The mock carries a 1.0-second constant TT-TDB dataset plus a
     * constant 2 AU Mars, so the file leg is unmistakable (the model
     * gives milliseconds) and the file-TT × apparent-place combination
     * is pinned. */
    {
        enum { QDBL = 360 };
        static unsigned char qhdr[QDBL * 8];
        static double qcons[QDBL];
        static double qvals[QDBL];
        static const jpl_date qtt = {2451545.5, 0.0};
        jpl_test_mock qm;
        jpl_ephemeris *qe = NULL;
        jpl_date qtdb, mtdb;
        jpl_state qs;
        size_t k;
        memset(qhdr, 0, sizeof qhdr);
        memcpy(qhdr, "policy file TT-TDB", 19);
        test_mock_header(qhdr, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(qhdr, 3, 3, 1, 1);
        test_put_ptr(qhdr, 14, 300, 2, 1);
        for (k = 0; k < QDBL; ++k) qcons[k] = 0.0;
        qcons[0] = 1.0;
        qcons[1] = 2.0;
        for (k = 0; k < QDBL; ++k) qvals[k] = 0.0;
        qvals[0] = 2451545.0;
        qvals[1] = 2451546.0;
        qvals[2] = 2.0 * JPL_TEST_AU_KM;
        qvals[299] = 1.0;
        qvals[300] = 0.0;
        memset(&qm, 0, sizeof qm);
        qm.record_doubles = QDBL;
        qm.advertised_records = 1;
        qm.denum = 441;
        qm.constant_count = 2;
        qm.start[0] = 2451545.0;
        qm.end[0] = 2451546.0;
        qm.step_days = 1.0;
        qm.au_km = JPL_TEST_AU_KM;
        qm.emrat = JPL_TEST_MASS_RATIO;
        qm.header = qhdr;
        qm.constants = qcons;
        qm.records = qvals;
        JPL_TEST_CHECK(test_write_mock("jpl_test_policy.bin", &qm) == 0);
        JPL_TEST_CHECK(jpl_open("jpl_test_policy.bin", &qe) == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(qe) != 0);
        /* File leg wins: policy lands ~1 s before TT, far from the model. */
        JPL_TEST_CHECK(jpl_tt_to_tdb(&qtt, &mtdb) == JPL_OK);
        JPL_TEST_CHECK(jpl_tt_to_tdb_policy(qe, &qtt, &qtdb) == JPL_OK);
        JPL_TEST_CHECK(fabs((qtdb.hi + qtdb.lo) - (2451545.5 - 1.0 / 86400.0)) < 1e-9);
        JPL_TEST_CHECK(fabs((qtdb.hi + qtdb.lo) - (mtdb.hi + mtdb.lo)) > 1e-9);
        /* Corrupt file data keeps the model result: the file is an
         * enhancement, never a new failure mode. */
        qvals[299] = NAN;
        JPL_TEST_CHECK(test_write_mock("jpl_test_policy_bad.bin", &qm) == 0);
        {
            jpl_ephemeris *qb = NULL;
            jpl_date btdb;
            JPL_TEST_CHECK(jpl_open("jpl_test_policy_bad.bin", &qb) == 0);
            JPL_TEST_CHECK(jpl_tt_to_tdb_policy(qb, &qtt, &btdb) == JPL_OK);
            JPL_TEST_CHECK(btdb.hi == mtdb.hi && btdb.lo == mtdb.lo);
            jpl_close(qb);
            JPL_TEST_CHECK(remove("jpl_test_policy_bad.bin") == 0);
        }
        /* File-TT × apparent-place golden: constant 2 AU Mars at
         * light-time-corrected SSB geometry. */
        JPL_TEST_CHECK(jpl_planet_position(qe, &qtt, JPL_MARS, JPL_SSB, &qs) == JPL_OK);
        JPL_TEST_CHECK(fabs(qs.x - 2.0) < 1e-9);
        JPL_TEST_CHECK(fabs(qs.y) < 1e-12 && fabs(qs.z) < 1e-12);
        JPL_TEST_CHECK(fabs(qs.vx) < 1e-12 && fabs(qs.vy) < 1e-12 && fabs(qs.vz) < 1e-12);
        jpl_close(qe);
        JPL_TEST_CHECK(remove("jpl_test_policy.bin") == 0);
    }
    puts("tt-tdb tests passed");
    return 0;
}
