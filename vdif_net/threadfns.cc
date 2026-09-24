// implementations of the threadfunctions
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
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

#include <fcntl.h>
#include <arpa/inet.h>
#include <threadfns.h>
#include <limits>
#include <runtime.h>
#include <threadutil.h>
#include <getsok.h>
#include <dosyscall.h>
#include <evlbidebug.h>

using namespace std;

DEFINE_EZEXCEPT(netreaderexception)

networkargs::networkargs() :
    allow_variable_block_size( false ), rteptr( 0 ), streamID( 0 )
{}
networkargs::networkargs(runtime* r, bool avbs, unsigned int stream):
    allow_variable_block_size( avbs ), rteptr( r ), streamID( stream )
{ ASSERT_NZERO(rteptr); netparms = rteptr->netparms; }
networkargs::networkargs(runtime* r, const netparms_type& np, bool avbs, unsigned int stream):
    allow_variable_block_size( avbs ), rteptr( r ), streamID( stream ), netparms( np )
{ ASSERT_NZERO(rteptr); }

fdreaderargs::fdreaderargs():
    fd( -1 ), doaccept( false ),
    rteptr( 0 ), threadid( 0 ),
    blocksize( 0 ), pool( 0 ),
    start( 0 ), end( 0 ), finished( false ), run( false ),
    max_bytes_to_cache( numeric_limits<uint64_t>::max() ),
    tag( 0 ),
    allow_variable_block_size( false )
{}
fdreaderargs::~fdreaderargs() {
    delete pool;     pool = 0;
    delete threadid; threadid = 0;
    fd = -1;
}
bool fdreaderargs::flush( void ) {
    int rv = 0;
    if( fd>-1 && (rv=::fsync(fd))!=0 ) {
        DEBUG(2, "fdreaderargs::flush/fsync() failed - " << evlbi5a::strerror(errno) << std::endl);
    }
    return rv == 0;
}

off_t fdreaderargs::get_start() {
    return start;
}
off_t fdreaderargs::get_end() {
    return end;
}
off_t fdreaderargs::get_file_size( void ) {
    off_t   rv = 0;
    DEBUG(4, "get_file_size: fd=" << fd << endl);
    if( fd>=0 ) {
        const off_t  current = ::lseek(fd, 0, SEEK_CUR);
        rv = ::lseek(fd, 0, SEEK_END);
        DEBUG(4, "       current=" << current << " rv=" << rv << " " << ((rv<0) ? evlbi5a::strerror(errno) : "") << endl)
        ::lseek(fd, current, SEEK_SET);
    }
    return rv;
}
void fdreaderargs::set_start(off_t s) {
    start = s;
}
void fdreaderargs::set_end(off_t e) {
    end = e;
}
bool fdreaderargs::is_finished() {
    return finished;
}
void fdreaderargs::set_run(bool newval) {
    run = newval;
}
uint64_t fdreaderargs::get_bytes_to_cache() {
    return max_bytes_to_cache;
}
void fdreaderargs::set_bytes_to_cache(uint64_t b) {
    max_bytes_to_cache = b;
}
void fdreaderargs::set_variable_block_size( bool b ) {
    allow_variable_block_size = b;
}

void close_filedescriptor(fdreaderargs* fdreader) {
    ASSERT_COND(fdreader);
    const string proto   = fdreader->netparms.get_protocol();
    int (*close_fn)(int) = &::close;

    if( proto!="udps" ) {
        DEBUG(-1, "close_filedescriptor: proto must be udps" << endl);
        exit(1);
    }

    if( fdreader->fd!=-1 ) {
        // This used to be an "ASSERT()" but then if we fail to close, the
        // ->fd member doesn't get set to '-1' so this keeps repeating
        // if "close_filedescriptor()" is called > once
        if( close_fn(fdreader->fd)!=0 ) {
            DEBUG(-1, "Failed to close fd#" << fdreader->fd << " (" << proto << ") - if UDT, may already be closed" << endl);
        } else {
            DEBUG(3, "close_filedescriptor: closed fd#" << fdreader->fd << endl);
        }
    }
    fdreader->fd = -1;
    if( fdreader->threadid!=0 ) {
        int rv = ::pthread_kill(*fdreader->threadid, SIGUSR1);

        // only acceptable returnvalues are 0 (=success) or ESRCH,
        // which means the thread has already terminated.
        if( rv!=0 && rv!=ESRCH ) {
            DEBUG(-1, "close_network: FAILED to SIGNAL THREAD - " << evlbi5a::strerror(rv) << endl);
        }
    }
    for(threadset_type::iterator p=fdreader->threads.begin(); p!=fdreader->threads.end(); p++) {
        // Make sure we don't do it twice
        if( fdreader->threadid && ::pthread_equal(*fdreader->threadid, *p) )
            continue;

        int rv = ::pthread_kill(*p, SIGUSR1);

        // only acceptable returnvalues are 0 (=success) or ESRCH,
        // which means the thread has already terminated.
        if( rv!=0 && rv!=ESRCH ) {
            DEBUG(-1, "close_network: FAILED to SIGNAL THREAD - " << evlbi5a::strerror(rv) << " (from threadset)" << endl);
        }
    }
}



// create a networkserver from the settings
// in "networkargs.(runtime_short*)->netparms_type"
// if protocol==rtcp (reverse tcp) it will be an
// outgoing connection.
fdreaderargs* net_server(networkargs net) {
    // get access to the actual network parameters
    const netparms_type&  np = net.netparms;
    const string          proto = np.get_protocol();
    unsigned int          olen( sizeof(np.sndbufsize) );
    // we're supposed to deliver a fresh instance of one of these
    fdreaderargs*         rv = new fdreaderargs();

    if ( proto != "udps") {
        DEBUG(-1, "netserver: proto must be udps" << endl);
        exit(1);
    }

    // copy over the runtime_short pointer
    rv->rteptr    = net.rteptr;
    rv->blocksize = np.get_blocksize();
    rv->netparms  = np;

    // copy over the variable block size allowingness
    rv->allow_variable_block_size = net.allow_variable_block_size;
    


    rv->fd = getsok(np.get_port(), proto, np.get_host());


    return rv;
}
