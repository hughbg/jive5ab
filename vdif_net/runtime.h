#ifndef RUNTIME_H
#include <string>
#include <pthread.h>
#include <netparms.h>
#include <transfermode.h>
#include <chainstats.h>
#include <block.h>
#include <bqueue.h>


struct runtime {
    unsigned rd_size;
    unsigned blocksize;
    unsigned port;
    netparms_type   netparms;
    unsigned int framesize;

    chainstats_type        statistics;
    chain                  processingchain;


    std::string host;   // for ACK

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

    runtime();

    void validate( void ) const;

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

