// reading UDP-with-sequence number (fill in missing pkts)
// Copyright (C) 2007-2023 Marjolein Verkouter
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
// Author:  Marjolein Verkouter - verkouter@jive.eu
//          Joint Institute for VLBI in Europe
//          P.O. Box 2
//          7990 AA Dwingeloo
#ifndef JIVE5A_THREADFNS_UDPSREADER_H
#define JIVE5A_THREADFNS_UDPSREADER_H

#include <chain.h>
#include <block.h>
#include <threadfns.h>
#include <threadfns/do_push_block.h>
#include <evlbidebug.h>
#include <dosyscall.h>

#include <list>
#include <string>
#include <sstream>
#include <iostream>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

// Non-templated udps reader functions
// only the high-level udpsreader will be templated to output
// blocks or tagged blocks - the lowest level readers just
// deliver blocks
void udpsreader_bh(outq_type<block>* outq, sync_type< sync_type<fdreaderargs> >* argsargs);
void udpsreader_th_nonzeroeing(inq_type<block>* inq, outq_type<block>* outq, sync_type<runtime*>* args);


// The actual udpsreader does nothing but build up a local processing chain
template <typename Item>
void udpsreader(outq_type<Item>* outq, sync_type<fdreaderargs>* args) {
    // Here we do the pre-check that everything points at something
    chain         c;
    runtime*      rteptr = 0;
    fdreaderargs* network = args->userdata;

    if( network )
        rteptr = network->rteptr; 
    ASSERT_COND( rteptr && network );

    // Before diving in too deep  ...
    // this asserts that all sizes make sense and meet certain constraints
    RTEEXEC(*rteptr, rteptr->validate());

    DEBUG(2, "udpsreader/manager starting" << std::endl);
    // Build local processing chain
    // If we're actually reading UDPS-with-no-reordering we only need
    // to change the bottom half - the bit that does the physical readin' :-)
    c.add(&udpsreader_bh, 2, args);
    c.add(&udpsreader_th_nonzeroeing, 2, rteptr);
    c.run();
    // and wait until it's done ...
    c.wait();
    // Now grab a lock on the sync type and inform them we've really
    // finished. There may be a cancel function waiting for this condition
    // to happen
    args->lock();
    network->finished = true;
    args->cond_broadcast();
    args->unlock();
    DEBUG(2, "udpsreader/manager done" << std::endl);
}

#endif
