// methods that create/configure sockets as per what we think are sensible defaults
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
#ifndef JIVE5A_GETSOK_H
#define JIVE5A_GETSOK_H

#include <string>

// Get a socket for incoming connections.
// The returned filedescriptor is in blocking mode.
//
// You *must* specify the port/protocol. Optionally specify
// a local interface to bind to. If left empty (which is
// default) bind to all interfaces.
int getsok(unsigned short port, const std::string& proto, const std::string& local = "");

// C++ does not have 'finally' keyword but instead you're supposed to 
// use the RAII thingamabob. It's more idiomatic so we do just that.
// Closes fd using the correct API call (system or libudt)
struct scopedfd {
    scopedfd(int (*closefn)(int) /*const std::string& proto*/);
    scopedfd(int fd, int (*closefn)(int) /*const std::string& proto*/);

    ~scopedfd();

    int   mFileDescriptor;
    int (*mCloseFn)(int);
    /*const std::string mProto;*/
};

#endif
