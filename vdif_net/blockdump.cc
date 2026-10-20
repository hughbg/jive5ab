#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <block.h>
#include <dosyscall.h>
#include <evlbidebug.h>

void blockdump(const char *fname, block b) {
    struct iovec*     chunks = new struct iovec[16];
    ssize_t                    bcnt;
    size_t            nchunk;
    ssize_t                    rv;
    struct iovec*              cptr;
    block::const_iterator bptr;
    int fd;

    // FIXME: FIXME if there are more than 16 entries in the block, this
    //              will fail!
    for( bptr=b.begin(), cptr=&chunks[0], nchunk=0, bcnt=0;
            bptr!=b.end() && nchunk<16;
            cptr++, bptr++, nchunk++) {
        cptr->iov_base  = bptr->iov_base;
        cptr->iov_len   = bptr->iov_len;
        bcnt           += (ssize_t)bptr->iov_len;
    }
    if ( (fd = creat(fname, S_IRUSR|S_IWUSR)) == -1 ) {
        lastsyserror_type lse;
        DEBUG(0, "blockdump: failed to write create file " << fname << std::endl);
        return;
    }


    // DO NOT ENTER A BLOCKING SYSTEMCALL WITH A LOCK HELD!
    if( (rv=::writev(fd, chunks, nchunk))!=(ssize_t)bcnt ) {
        lastsyserror_type lse;
        DEBUG(0, "blockdump: failed to write " << bcnt << " bytes "
                    << lse << " (only " << rv << " written, nchunk=" << nchunk << ")" << std::endl);
    }
    close(fd);
}

void rawdump(const char *fname, const unsigned char *p, unsigned count) {
    int fd;
    ssize_t rv;

    if ( (fd = creat(fname, S_IRUSR|S_IWUSR)) == -1 ) {
        lastsyserror_type lse;
        DEBUG(0, "rawdump: failed to write create file " << fname << std::endl);
        return;
    }
    if( (rv=::write(fd, p, count))!=count) {
        lastsyserror_type lse;
        DEBUG(0, "rawdump: fail to write to file " << fname << std::endl);
    }
    close(fd);
}



