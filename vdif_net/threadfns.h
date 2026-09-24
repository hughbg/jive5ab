// available thread-functions
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
#ifndef JIVE5A_THREADFNS_H
#define JIVE5A_THREADFNS_H
#include <set>
#include <runtime.h>
#include <blockpool.h>
#include <netparms.h>
#include <pthread.h>

DECLARE_EZEXCEPT(netreaderexception)

typedef std::set<pthread_t> threadset_type;
struct fdreaderargs {
    int             fd;
    bool            doaccept;
    runtime*        rteptr;
    pthread_t*      threadid;
    unsigned int    blocksize;
    netparms_type   netparms;
    blockpool_type* pool;
    off_t           start;
    off_t           end;
    bool            finished;
    bool            run;
    uint64_t        max_bytes_to_cache;
    unsigned int    tag;
    threadset_type  threads;

    // allow the producer thread to produce variable sized blocks
    // this allows eg disk2file to copy the complete file and not be rounded
    // down to integer multiples of blocksize
    bool            allow_variable_block_size;

    fdreaderargs();
    ~fdreaderargs();

    bool     flush( void );
    off_t    get_start();
    off_t    get_end();
    void     set_start(off_t s);
    void     set_end(off_t e);
    bool     is_finished();
    void     set_run(bool r);
    void     set_bytes_to_cache(uint64_t b);
    uint64_t get_bytes_to_cache();
    void     set_variable_block_size( bool b );
    off_t    get_file_size( void );

    private:
        fdreaderargs(fdreaderargs const&);
        fdreaderargs const& operator=(fdreaderargs const&);
};

struct networkargs {
    bool               allow_variable_block_size;
    runtime*           rteptr;
    unsigned int       streamID;
    netparms_type      netparms;

    networkargs();
    networkargs(runtime* r, bool avbs=false, unsigned int stream=0);
    networkargs(runtime* r, const netparms_type& np, bool avbs=false, unsigned int stream=0);
};

void close_filedescriptor(fdreaderargs* fdreader);

fdreaderargs* net_server(networkargs net);

#endif
