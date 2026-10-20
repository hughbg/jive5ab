// This process lives outside the chain/queue system and extracts vdif frames in blocks from memory (interchain_queue)

#include <vdif_queue_reader.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

iovec_result VDIFQueueReader::read_block() {
    block b;
    struct iovec*     chunks = new struct iovec[16];
    ssize_t                    bcnt;
    size_t            nchunk;
    struct iovec*              cptr;
    block::const_iterator bptr;
    int fd;

    bool did_pop = interchain_queue->pop(b);
    if ( !did_pop ) return std::make_pair((struct iovec*)NULL, (ssize_t)0);


    for( bptr=b.begin(), cptr=&chunks[0], nchunk=0, bcnt=0;
            bptr!=b.end() && nchunk<16;
            cptr++, bptr++, nchunk++) {
        cptr->iov_base  = bptr->iov_base;
        cptr->iov_len   = bptr->iov_len;
        bcnt           += (ssize_t)bptr->iov_len;
    }

    return std::make_pair(chunks, nchunk);
}
