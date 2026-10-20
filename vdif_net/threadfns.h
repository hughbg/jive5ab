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
#include <runtime.h>

struct fakerargs {
    runtime*        rteptr;
    unsigned char*  buffer;
    size_t          size;
    size_t          framesize;
    blockpool_type* framepool;
    uint32_t        ref_time;

    void init_vdif_frame();
    void update_vdif_frame(time_t);
    void init_frame();
    void update_frame(time_t);

    fakerargs();
    fakerargs(runtime* rte);

    // calls delete [] on buffer.
    ~fakerargs();
};

using namespace std;

void faker(inq_type<block>*, outq_type<block>*, sync_type<fakerargs>*);
void close_filedescriptor(runtime* fdreader);

runtime* net_server(runtime*);

#endif
