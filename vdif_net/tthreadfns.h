// available templated thread-functions
// Copyright (C) 2007-2008 Harro Verkouter
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// Author:  Harro Verkouter - verkouter@jive.nl
//          Joint Institute for VLBI in Europe
//          P.O. Box 2
//          7990 AA Dwingeloo
#ifndef JIVE5A_TTHREADFNS_H
#define JIVE5A_TTHREADFNS_H


#include <sys/uio.h>
#include <sys/socket.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <sciprint.h>
#include <chain.h>
#include <runtime.h>
#include <dosyscall.h>
#include <evlbidebug.h>

// Generic filedescriptor writer.
// whatever gets popped from the inq gets written to the filedescriptor.
// leave intelligence up to other steps.
template <typename T>
void fdwriter(inq_type<T>* inq, sync_type<runtime>* args) {
    bool              stop = false;
    size_t            nchunk;
    uint64_t          nbyte = 0;
    struct iovec*     chunks = new struct iovec[16];
    runtime*     rteptr = args->userdata;


    ASSERT_COND(rteptr!=0);
    // first things first: register our threadid so we can be cancelled
    // if the network is to be closed and we don't know about it
    // (under linux, closing a filedescriptor in one thread does not
    // make another thread, blocking on the same fd, wake up with
    // an error. b*tards).
    install_zig_for_this_thread(SIGUSR1);
    SYNCEXEC(args,
             stop              = args->cancelled;
             delete rteptr->threadid;
             rteptr->threadid = new pthread_t(::pthread_self()));

    // since we ended up here we must be connected!
    // we do not clear the wait flag since we're not the one guarding that
    RTEEXEC(*rteptr,
            rteptr->transfersubmode.set(connected_flag));

    if( stop ) {
        DEBUG(0, "fdwriter: got stopsignal before actually starting" << std::endl);
        SYNCEXEC(args, delete rteptr->threadid; rteptr->threadid=0);
        return;
    }

    DEBUG(0, "fdwriter: writing to fd=" << rteptr->fd << std::endl);

    uint64_t bytes_in_cache = 0;
    // blind copy of incoming data to outgoing filedescriptor
    while( true ) {
        T b;
        if ( !inq->pop(b) ) {
            break;
        }
        ssize_t                    bcnt;
        ssize_t                    rv;
        struct iovec*              cptr;
        typename T::const_iterator bptr;

        // FIXME: FIXME if there are more than 16 entries in the block, this
        //              will fail!
        for( bptr=b.begin(), cptr=&chunks[0], nchunk=0, bcnt=0;
             bptr!=b.end() && nchunk<16;
             cptr++, bptr++, nchunk++) {
            cptr->iov_base  = bptr->iov_base;
            cptr->iov_len   = bptr->iov_len;
            bcnt           += (ssize_t)bptr->iov_len;
        }
        // DO NOT ENTER A BLOCKING SYSTEMCALL WITH A LOCK HELD!
        if( (rv=::writev(rteptr->fd, chunks, nchunk))!=(ssize_t)bcnt ) {
            lastsyserror_type lse;
            DEBUG(0, "fdwriter: fail to write " << bcnt << " bytes "
                     << lse << " (only " << rv << " written, nchunk=" << nchunk << ")" << std::endl);
            break;
        }
        
        nbyte   += (uint64_t)bcnt;

        bytes_in_cache += (uint64_t)bcnt;

        if ( bytes_in_cache > rteptr->max_bytes_to_cache ) {
            // flush every now and then, to prevent the kernel to built up a huge cache
            if ( (rv = ::fsync( rteptr->fd )) != 0 ) {
                DEBUG(-1, "fdwriter: sync failed: " << evlbi5a::strerror(errno) << std::endl);
            }
            bytes_in_cache = 0;
        }
    }
    // Issue a shutdown for writing - we know we're not going to write data
    // anymore. Also issue a read - wait until the other end has closed as
    // well. In case the sokkit was already geclosed, this finishes
    // immediately (I hope :D)
    if( ::shutdown(rteptr->fd, SHUT_WR)==0 ) {
        char    c;
        if( ::read(rteptr->fd, &c, 1) ) {}
    }
    // We're not going to block on the fd anymore so we should unregister
    // ourselves from receiving signals
    SYNCEXEC(args, delete rteptr->threadid; rteptr->threadid = 0);

    delete [] chunks;
    DEBUG(0, "fdwriter: stopping. wrote "
             << nbyte << " (" << byteprint((double)nbyte,"byte") << ")"
             << std::endl);
    rteptr->finished = true;
}

#endif
