#include <cassert>
#include <runtime.h>


#define UNCONSTRAINED ((unsigned int)-1)

#define GETCONSTRAINT(x) x
#define ISSET(x) x != UNCONSTRAINED
#define ASSERT(x) assert(x)

runtime::runtime():
    rd_size(UNCONSTRAINED), blocksize(UNCONSTRAINED), port(UNCONSTRAINED), host(""),
    interchain_source_queue( NULL ), transfermode( no_transfer ), transfersubmode( transfer_submode() )
{
    PTHREAD_CALL( ::pthread_mutex_init(&rte_mutex, 0) );

}

void runtime::validate( void ) const {
    // those MUST have values
    ASSERT( ISSET(blocksize) );
    ASSERT( ISSET(rd_size) );
    ASSERT( ISSET(port) );

    // these may not be 0
    ASSERT( GETCONSTRAINT(blocksize)>0 );
    ASSERT( GETCONSTRAINT(rd_size)>0 );
    ASSERT( GETCONSTRAINT(rd_size)>=49152);

    // these must be a multple of 8
    ASSERT( (GETCONSTRAINT(blocksize)%8)==0 );
    //ASSERT( (GETCONSTRAINT(rd_size)%8)==0 );

    // blocksize must be an integral multiple of readsize (including the
    // multiple "1").
    ASSERT( GETCONSTRAINT(blocksize)>=GETCONSTRAINT(rd_size) );
    ASSERT( (GETCONSTRAINT(blocksize)%GETCONSTRAINT(rd_size))==0 );

    ASSERT( GETCONSTRAINT(interchain_source_queue)!=NULL );

    ASSERT( GETCONSTRAINT(transfermode)!=no_transfer );
    //ASSERT( GETCONSTRAINT(transfersubmode)!=no_transfer );

    // framesize constrained? in that case frame/block should be integral
    // multiples! we do not care which one is the bigger one either way of
    // being a multiple is good enough for us
    if( ISSET(framesize) ) {
        // if framesize set it better be non-zero and a multiple of 8
        ASSERT( GETCONSTRAINT(framesize)>0 );
        ASSERT( (GETCONSTRAINT(framesize)%8)==0 );
        // Uncompressed readsize divides an integral amount
        // of times into the framesize
        ASSERT( (GETCONSTRAINT(framesize)%GETCONSTRAINT(rd_size))==0 );
// After discussion between BobE and HarroV it seems
// that the following constraints serve no purpose.
// For now we relax them.
#if 0
        if( GETCONSTRAINT(framesize)>GETCONSTRAINT(blocksize) ) {
            ASSERT( (GETCONSTRAINT(framesize)%GETCONSTRAINT(blocksize))==0 );
        } else {
            ASSERT( (GETCONSTRAINT(blocksize)%GETCONSTRAINT(framesize))==0 );
        }
#endif
    }
    return;
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
