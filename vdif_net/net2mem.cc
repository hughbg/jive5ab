// Copyright (C) 2007-2013 Harro Verkouter
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

#include <cstdio>
#include <threadfns/netreader.h>
#include <interchainfns.h>
#include <transfermode.h>


void net2mem_fn(runtime& rte ) {
    const transfer_type rtm( string2transfermode("net2mem") );

    std::cout << "net2mem open" << std::endl;

    chain c;

    std::cout << "net2mem chaining netreader" << std::endl;
    c.register_cancel( c.add(&netreader<block>, 10, &net_server, networkargs(&rte)),
                        &close_filedescriptor);
    // And write to mem
    c.add( queue_writer, queue_writer_args(&rte) );

    // reset statistics counters
    rte.statistics.clear();
    rte.transfersubmode.clr_all();

    rte.transfermode = rtm;
    rte.processingchain = c;
    rte.processingchain.run();

}
