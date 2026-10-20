#ifndef RUNTIME_H
#include <string>
#include <pthread.h>
#include <netparms.h>
#include <set>
#include <transfermode.h>
#include <chainstats.h>
#include <block.h>
#include <blockpool.h>
#include <bqueue.h>

#include <cassert>

typedef std::set<pthread_t> threadset_type;

class runtime {
public:
    // these were in netparms_type
    unsigned int port;    // network port
    unsigned int nblock;    // not sure what this is for apart from general buffer or spool space. e.g. 8
    int ackPeriod;

    chain                  processingchain;

    std::string host;   // for ACK

    // stats and transfermode are kept in to reduce changes to other code that uses them (interchains).
    // Probably can be removed otherwise.
    chainstats_type        statistics;

    // The global transfermode and submode/status
    transfer_type          transfermode;
    transfer_submode       transfersubmode;


    // shared access between multiple threads.
    // please grab/release lock. Use the scoped lock to make it automatic.
    void lock( void );
    void unlock( void );

    // The mutex for locking
    pthread_mutex_t               rte_mutex;

    // the queue that can be used to communicate data between runtimes
    bqueue<block>*         interchain_source_queue;

    // these were in fdreaderargs
    int    fd;         
    pthread_t*      threadid;         // pthreads not VDIF threads
    threadset_type  threads;
    unsigned int    tag;
    bool finished;
    blockpool_type *pool;
    uint64_t max_bytes_to_cache;
    
    bool check_vdif_spec();


    // The policy is that the stuff relating to VDIF configuration MUST be set explicitly.
    
    // setters

    void set_framesize(const unsigned fs) {
        assert(fs>0 && fs%8==0);
        framesize_ = fs;
        framesize_is_set = true;
    }

    void set_blocksize(const unsigned bs) {
        assert(bs>0 && bs%framesize_==0);
        blocksize_ = bs;
        blocksize_is_set = true;
    }

    void set_nchannel(const unsigned nc) {
        assert(nc>0 && (nc & (nc - 1)) == 0);
        nchannel_ = nc;
        nchannel_is_set = true;
    }

    void set_bits_per_sample(const unsigned bps) {
        assert(bps>0 && bps<=32);
        bits_per_sample_ = bps;
        bits_per_sample_is_set = true;
    }

    // getters
    unsigned const get_framesize() {
        assert(framesize_is_set);
        return framesize_;
    }


    unsigned const get_blocksize() {
        assert(blocksize_is_set);
        return blocksize_;
    }

    unsigned const get_nchannel() {
        assert(nchannel_is_set);
        return nchannel_;
    }

    unsigned const get_bits_per_sample() {
        assert(bits_per_sample_is_set);
        return bits_per_sample_;
    }


    runtime();


private:

    // were in runtime sizes
    unsigned framesize_;     // VDIF frame size (not including a sequence number)
    // blocksize is assumed to be 16 in various places in the code. FIX
    unsigned blocksize_;     // multiple of read_size eg. 16 x read_size

    unsigned nchannel_;          // must be power of 2
    unsigned bits_per_sample_;


    // Stuff relating to VDIF format must be explicitly set

    bool framesize_is_set, blocksize_is_set, nchannel_is_set, bits_per_sample_is_set;

};

struct scopedrtelock {
    public:
        scopedrtelock(runtime& rte);
        ~scopedrtelock();
    private:
        // nice. a reference as datamember. since the lifetime of this
        // object is small and its undefaultcreatable/copyable/assignable
        // that's ... doable.
        runtime&   rteref;

        // no default c'tor
        scopedrtelock();
        // nor copy
        scopedrtelock(const scopedrtelock&);
        // nor assignment
        const scopedrtelock& operator=(const scopedrtelock&);
};

// statements 'e' will only be evaluated if an exception is thrown!
// they do NOT behave like a 'finally'
#define RTE3EXEC(r, f, e) \
    try { \
        scopedrtelock  sC0p3dL0KkshZh(r); \
        f;\
    }\
    catch (...) { \
        e;\
        throw; \
    }

#define RTEEXEC(r, f) \
    RTE3EXEC(r, f, ;)

#define RUNTIME_H
#endif

