#include <interchainfns.h>
#include <interchain.h>
#include <evlbidebug.h>
#include <dosyscall.h>
#include <iostream>
#include <memory>

using namespace std;

// make the interchain queue contain at max 512 MB, by default
const unsigned int INTERCHAIN_QUEUE_SIZE = 512 * 1024 * 1024; 

queue_writer_args::queue_writer_args() : rteptr(NULL), disable(false) {}
queue_writer_args::queue_writer_args(runtime* r) : rteptr(r), disable(false) {}
queue_writer_args::~queue_writer_args() {
    // clean up
    if ( disable ) {
        interchain_queues_disable();
    }
}


void queue_writer(inq_type<block>* inq, sync_type<queue_writer_args>* args) {
    ASSERT_COND( args && args->userdata && args->userdata->rteptr );
    queue_writer_args* qwargs = args->userdata;
    runtime* rteptr = qwargs->rteptr;

    RTEEXEC(*rteptr, rteptr->check_vdif_spec());
    const unsigned int blocksize = rteptr->get_blocksize();

    // enable the queue between the chains, remember to disable it when done
    // note: we must have at least 1 entry! (With the advent of FlexBuff,
    // 'blocksize' could become *huge* such that
    // INTERCHAIN_QUEUE_SIZE/blocksize would become 0 ...)
    interchain_queues_resize_enable_push( std::max(INTERCHAIN_QUEUE_SIZE / blocksize, 1u) );
    args->userdata->disable = true;

    DEBUG(0, "queue_writer: starting" << endl);
    do {
        block b;

        if ( !inq->pop(b) ) {
            break;
        }
        interchain_queues_try_push(b);
    } while(true);
    DEBUG(0, "queue_writer: stopping" << endl);

}

