
#include <runtime.h>


runtime::runtime():
    framesize_(0), blocksize_(0), nchannel_(0), bits_per_sample_(0), nblock(0), port(0), host(""),
    interchain_source_queue( NULL ), threadid(NULL), fd(-1), transfermode( no_transfer ), transfersubmode( transfer_submode() ),
    finished( false ), pool( NULL), ackPeriod(10), max_bytes_to_cache(256*1024*1024),
    framesize_is_set(false), blocksize_is_set(false), nchannel_is_set(false), bits_per_sample_is_set(false)
{
    PTHREAD_CALL( ::pthread_mutex_init(&rte_mutex, 0) );

}

bool runtime::check_vdif_spec() {
    assert(framesize_is_set);
    assert(blocksize_is_set);
    assert(nchannel_is_set);
    assert(bits_per_sample_is_set);
    
    return true;
}

// scoped lock for the runtime
scopedrtelock::scopedrtelock(runtime& rte):
    rteref(rte)
{
  rteref.lock();
}

scopedrtelock::~scopedrtelock() {
    rteref.unlock();
}

void runtime::lock( void ) {
    PTHREAD_CALL( ::pthread_mutex_lock(&rte_mutex) );
}
void runtime::unlock( void ) {
    PTHREAD_CALL( ::pthread_mutex_unlock(&rte_mutex) );
}
