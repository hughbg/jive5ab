// templated threaded read functions
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
#ifndef JIVE5A_THREADFNS_NETREADER_H
#define JIVE5A_THREADFNS_NETREADER_H


// own code
#include <getsok.h>
#include <runtime.h>
#include <evlbidebug.h>
#include <threadfns/udpsreader.h>
//#include <threadfns/do_push_block.h>
//#include <threadfns.h>

// std c++
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <typeinfo>

// std C
#include <pthread.h>
#include <signal.h>


// Transparent support for pushing tagged items or not
// - No tagged itmes used.
template <typename Item>
void netreader(outq_type<Item>* outq, sync_type<fdreaderargs>* args) {
    static const std::string myself( std::string("netreader<") + typeid(Item).name() + ">: " );
    // deal with generic networkstuff
    bool                   stop;
    fdreaderargs*          network = args->userdata;
    const std::string      proto = network->netparms.get_protocol();
    scopedfd               acceptedfd( &::close );

    DEBUG(0, "netreader[" << ::pthread_self() << "]: starting" << std::endl);
    // first things first: register our threadid so we can be cancelled
    // if the network (if 'fd' refers to network that is) is to be closed
    // and we don't know about it because we're in a blocking syscall.
    // (under linux, closing a filedescriptor in one thread does not
    // make another thread, blocking on the same fd, wake up with
    // an error. b*tards).
    // do the malloc/new outside the critical section. operator new()
    // may throw. if that happens whilst we hold the lock we get
    // a deadlock. we no like.
    SYNCEXEC(args, stop = args->cancelled);

    if ( proto != "udps") {
        DEBUG(-1, "netserver: proto must be udps" << std::endl);
        exit(1);
    }


    if( stop ) {
        DEBUG(0, myself << "stop signalled before we actually started" << std::endl);
        return;
    }


    // update submode flags
    RTEEXEC(*network->rteptr, 
            network->rteptr->transfersubmode.clr( wait_flag ).set( connected_flag ));

    // and delegate to appropriate reader
    udpsreader(outq, args);

    // We're definitely not going to block on any fd anymore so make rly
    // sure we're not receiving signals no more
    SYNCEXEC(args, delete network->threadid; network->threadid = 0; network->finished = true;);

    // update submode flags
    RTEEXEC(*network->rteptr, 
            network->rteptr->transfersubmode.clr( connected_flag ) );
}

#endif  // include guard
