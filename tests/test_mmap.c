#include "jpl.h"
#include "jpl_test_util.h"
#include <stdio.h>
int main(void){
    const char *path="jpl_test_mmap.bin";
    unsigned char rec[JPL_TEST_RECORD_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL,*c=NULL;
    const jpl_header *h; double vals[357]; size_t i;
    memcpy(rec,"mmap test",9);
    test_mock_header(rec,2451545.0,2451546.0,1.0,2);
    test_put_ptr(rec,11,2,178,1);
    f=fopen(path,"wb"); JPL_TEST_CHECK(f); JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
    memset(rec,0,sizeof rec); test_put_f64(rec,1.0); test_put_f64(rec+8,2.0);
    JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec);
    for(i=0;i<357;++i) test_put_f64(rec+i*8,(double)i);
    JPL_TEST_CHECK(fwrite(rec,1,sizeof rec,f)==sizeof rec); JPL_TEST_CHECK(fclose(f)==0);
    JPL_TEST_CHECK(jpl_open_mmap(path,&e)==JPL_OK);
    h=jpl_get_header(e); JPL_TEST_CHECK(h && h->constant_count==2 && jpl_record_count(e)==1);
    JPL_TEST_CHECK(jpl_ephemeris_clone(e,&c)==JPL_OK);
    JPL_TEST_CHECK(jpl_read_record(e,0,vals,357)==JPL_OK && vals[356]==356.0);
    JPL_TEST_CHECK(jpl_read_record(c,0,vals,357)==JPL_OK && vals[0]==0.0);
    /* mmap handle must be shareable: interleaved state reads on one handle
     * equal reads on a distinct clone (pins no-per-handle-mutation).
     * NOTE: uses a dedicated mock whose record bounds match header
     * coverage (the main mock above stores 0..356 as data, so its bounds
     * 0..1 cannot satisfy the jd window). */
    {
        const char *path2 = "jpl_test_mmap_share.bin";
        unsigned char rec2[JPL_TEST_RECORD_BYTES] = {0};
        FILE *f2;
        jpl_ephemeris *e2 = NULL, *c2 = NULL;
        double a[6], b[6];
        int k, same = 1;
        memcpy(rec2, "mmap share", 10);
        test_mock_header(rec2, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec2, 11, 2, 178, 1);
        f2 = fopen(path2, "wb"); JPL_TEST_CHECK(f2);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        memset(rec2, 0, sizeof rec2);
        test_put_f64(rec2, 1.0); test_put_f64(rec2 + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        memset(rec2, 0, sizeof rec2);
        test_put_f64(rec2 + 0 * 8, 2451545.0);
        test_put_f64(rec2 + 1 * 8, 2451546.0);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        JPL_TEST_CHECK(fclose(f2) == 0);
        JPL_TEST_CHECK(jpl_open_mmap(path2, &e2) == JPL_OK);
        JPL_TEST_CHECK(jpl_ephemeris_clone(e2, &c2) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e2, 2451545.5, JPL_NUTATIONS, 0, a) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(c2, 2451545.5, JPL_NUTATIONS, 0, b) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e2, 2451545.5, JPL_NUTATIONS, 0, a) == JPL_OK);
        for (k = 0; k < 6; ++k) if (a[k] != b[k]) same = 0;
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
      JPL_TEST_CHECK(jpl_read_record(fe,0,vals,357)==JPL_OK && vals[356]==356.0);
      JPL_TEST_CHECK(jpl_read_record(fc,0,vals,357)==JPL_OK && vals[0]==0.0);
      jpl_close(fc); jpl_close(fe); }
    JPL_TEST_CHECK(remove(path)==0);
    puts("mmap tests passed"); return 0;
}
