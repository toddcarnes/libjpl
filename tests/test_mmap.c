#include "jpl.h"
#include "jpl_test_util.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    const char *path="jpl_test_mmap.bin";
    unsigned char rec[JPL_TEST_RECORD_BYTES]={0}; FILE *f; jpl_ephemeris *e=NULL,*c=NULL;
    const jpl_header *h; double vals[357]; size_t i;
    memcpy(rec,"mmap test",9);
    test_mock_header(rec,2451545.0,2451546.0,1.0,2);
    test_put_ptr(rec,11,2,178,1);
    f=fopen(path,"wb"); assert(f); assert(fwrite(rec,1,sizeof rec,f)==sizeof rec);
    memset(rec,0,sizeof rec); test_put_f64(rec,1.0); test_put_f64(rec+8,2.0);
    assert(fwrite(rec,1,sizeof rec,f)==sizeof rec);
    for(i=0;i<357;++i) test_put_f64(rec+i*8,(double)i);
    assert(fwrite(rec,1,sizeof rec,f)==sizeof rec); assert(fclose(f)==0);
    assert(jpl_open_mmap(path,&e)==JPL_OK);
    h=jpl_get_header(e); assert(h && h->constant_count==2 && jpl_record_count(e)==1);
    assert(jpl_ephemeris_clone(e,&c)==JPL_OK);
    assert(jpl_read_record(e,0,vals,357)==JPL_OK && vals[356]==356.0);
    assert(jpl_read_record(c,0,vals,357)==JPL_OK && vals[0]==0.0);
    assert(jpl_ephemeris_clone(NULL,NULL)==JPL_ERROR_ARGUMENT);
    assert(jpl_open_mmap(NULL,NULL)==JPL_ERROR_ARGUMENT);
    jpl_close(c); jpl_close(e);
    { jpl_ephemeris *fe=NULL, *fc=NULL;
      assert(jpl_open(path,&fe)==JPL_OK);
      assert(jpl_ephemeris_clone(fe,&fc)==JPL_OK);
      assert(jpl_read_record(fe,0,vals,357)==JPL_OK && vals[356]==356.0);
      assert(jpl_read_record(fc,0,vals,357)==JPL_OK && vals[0]==0.0);
      jpl_close(fc); jpl_close(fe); }
    assert(remove(path)==0);
    puts("mmap tests passed"); return 0;
}
