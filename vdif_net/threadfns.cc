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



#include <signal.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <threadfns.h>
#include <runtime.h>
#include <getsok.h>
#include <dosyscall.h>
#include <evlbidebug.h>
#include <timezooi.h>


using namespace std;

DECLARE_EZEXCEPT(netreaderexception)
DEFINE_EZEXCEPT(netreaderexception)

struct modal {
    modal(int m) :
        mode(m)
    {}
    int mode;
};

#define OPENKEES(s, i, flag) \
    if( (i&flag)!=0 ) s << #flag << ",";

ostream& operator<<(ostream& os, const modal& m) {
    os << "<";
    OPENKEES(os, m.mode, O_RDONLY);
    OPENKEES(os, m.mode, O_WRONLY);
    OPENKEES(os, m.mode, O_CREAT);
    OPENKEES(os, m.mode, O_APPEND);
    OPENKEES(os, m.mode, O_TRUNC);
    OPENKEES(os, m.mode, O_RDWR);
    OPENKEES(os, m.mode, O_EXCL);
    os << ">";
    return os;
}

string tolower( const string& s ) {
    string                 retval;
    string::const_iterator cptr;

    for( cptr=s.begin(); cptr!=s.end(); cptr++ )
        retval.push_back( (char)::tolower(*cptr) );
    return retval;
}


// If the OS we're running on has O_LARGEFILE ... use that!
#ifdef O_LARGEFILE
    #define LARGEFILEFLAG  O_LARGEFILE
#else
    #define LARGEFILEFLAG  0
#endif

// <filename> should be
//   [/]path/to/file.name,<mode>
// where <mode> is
//    "r"   => open file readonly
//    "w"   => open file for writing, create it if necessary, truncate it
//    "a"   => open file for appending, create it if necessary
//    "n"   => create file for writing, fail if already exist
// the file will be opened in binary 
runtime *open_file(string filename, runtime* rteptr) {
    int               flag = LARGEFILEFLAG; // smoke'm if you got'em
    mode_t            mode = 0; // only used when creating
    string            openmode;
    string            actualfilename;
    string::size_type openmodeptr;

    openmodeptr = filename.find(",");
    ASSERT2_COND( openmodeptr!=string::npos,
                  SCINFO(" add ',<mode>' to the filename (r,w,a,n)") );

    openmode       = tolower(filename.substr(openmodeptr+1));
    actualfilename = filename.substr(0, openmodeptr);

    ASSERT2_COND( filename.size()>0,
                  SCINFO(" no actual filename given") );
    ASSERT2_COND( openmode.size()>0,
                  SCINFO(" no actual openmode (r,w,a) given") );
    
    if( openmode=="r" ) 
        flag |= O_RDONLY;
    else if( (openmode=="w") || (openmode=="n") ) {
        // for n we check first that the file doesn't exists
        if ( openmode == "n") {
            flag |= O_EXCL;
        }
        // create the file if it don't exist
        flag |= (O_WRONLY | O_CREAT | O_TRUNC);
        // file rw for us, r for everyone else
        mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    }
    else if( openmode=="a" ) {
        // create the file if it don't exist
        flag |= (O_WRONLY | O_CREAT | O_APPEND);
        // file rw for us, r for everyone else
        mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    } else {
        ASSERT2_COND( false, SCINFO("invalid openmode '" << openmode << "'") );
    }
    ASSERT2_COND( (rteptr->fd=::open(actualfilename.c_str(), flag, mode))!=-1,
                  SCINFO(actualfilename << ", flag=" << modal(flag) << ", mode=" << mode) );
    // we must potentially change ownership - this has already
    // been configured at startup such that we can blindly do this:
    //ASSERT2_ZERO(mk6info_type::fchown_fn(rv->fd, mk6info_type::real_user_id, -1),
    //             SCINFO("Failed to change ownership of new create file " << actualfilename));
    DEBUG(0, "open_file: opened " << actualfilename << " as fd=" << rteptr->fd << endl);

    return rteptr;
}


void close_filedescriptor(runtime* fdreader) {
    ASSERT_COND(fdreader);
    int (*close_fn)(int) = &::close;

     if( fdreader->fd!=-1 ) {
        // This used to be an "ASSERT()" but then if we fail to close, the
        // ->fd member doesn't get set to '-1' so this keeps repeating
        // if "close_filedescriptor()" is called > once
        if( close_fn(fdreader->fd)!=0 ) {
            DEBUG(-1, "close_filedescriptor: Failed to close fd#" << fdreader->fd << endl);
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


DECLARE_EZEXCEPT(fakerexception)
DEFINE_EZEXCEPT(fakerexception)




fakerargs::fakerargs():
    rteptr( 0 ), buffer( 0 ), framepool( 0 ), size(0), framesize(0), ref_time(0)
{}

fakerargs::fakerargs(runtime* rte):
    rteptr( rte ), buffer( 0 ), framepool( 0 ), size(0), framesize(0), ref_time(0)
{ 
  EZASSERT2(rteptr, fakerexception, EZINFO("Must have a non-null runtime pointer"));
  rteptr->check_vdif_spec();
}

fakerargs::~fakerargs() {
    delete [] buffer;
    delete framepool;
}

void
fakerargs::init_vdif_frame() {

    uint32_t *frame32;
    uint8_t ref_epoch;
    uint8_t log2nchans;
    struct tm tm;
    time_t clock;
    int i;
    
    framesize = rteptr->get_framesize();

    size = rteptr->get_blocksize();
    buffer = new unsigned char[size];
    frame32 = (uint32_t *)buffer;

    time(&clock);
    gmtime_r(&clock, &tm);
    tm.tm_mon = (tm.tm_mon < 6 ? 0 : 6);
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    ref_epoch = (tm.tm_year - 100) * 2 + tm.tm_mon / 6;
    ref_time = ::my_timegm(&tm);

    // complex data have half the number of channels. Don't allow complex.
    log2nchans = ((ffs(rteptr->get_nchannel()) - 1) & 0x1f);

    for (i = 0; i < 16; i++) {
      frame32[i * (framesize / 4) + 0] = 0x80000000;
      frame32[i * (framesize / 4) + 1] = (ref_epoch << 24) | i;
      frame32[i * (framesize / 4) + 2] = (log2nchans << 24) | (framesize / 8);
      frame32[i * (framesize / 4) + 3] = (((rteptr->get_bits_per_sample()-1) & 0x1f) << 26);
    }
}

void
fakerargs::update_vdif_frame(time_t clock)
{
    uint32_t *frame32 = (uint32_t *)buffer;
    int i;

    for (i = 0; i < 16; i++) {
      frame32[i * (framesize / 4) + 0] &= ~0x3fffffff;
      frame32[i * (framesize / 4) + 0] |= ((clock - ref_time) & 0x3fffffff);
    }
}

void fakerargs::init_frame()
{

    init_vdif_frame();
}

void fakerargs::update_frame(time_t clock)
{

    update_vdif_frame(clock);
}


void faker(inq_type<block>* inq, outq_type<block>* outq, sync_type<fakerargs>* args)
{
    runtime* rteptr;
    fakerargs* fakeargs = args->userdata;
    struct timespec tv;
    pop_result_type ret;
    int ntimeouts = 0;
    time_t clock = 0;

    // Assert we do have a runtime pointer!
    ASSERT2_COND(rteptr = fakeargs->rteptr, SCINFO("OH NOES! No runtime pointer!"));

    // Do the initializing of the fakerargs with the lock held
    SYNCEXEC(args,
             fakeargs->init_frame();
             if( fakeargs->framepool==0 )
                fakeargs->framepool = new blockpool_type(fakeargs->size, 4));

    while( true ) {
        block b;
#if !defined(__APPLE__)
        ::clock_gettime(CLOCK_REALTIME, &tv);
#else
        struct timeval  osx_doesnt_fucking_have_clockgettime_fucking_retards;
        ::gettimeofday(&osx_doesnt_fucking_have_clockgettime_fucking_retards, 0);
        tv.tv_sec  = osx_doesnt_fucking_have_clockgettime_fucking_retards.tv_sec;
        tv.tv_nsec = osx_doesnt_fucking_have_clockgettime_fucking_retards.tv_usec*1000;
#endif
        tv.tv_sec += 1;
        ret = inq->pop(b, tv);
        if (ret == pop_disabled)
            break;
        if (ret == pop_timeout) {
            if (ntimeouts++ > 1) {
                fakeargs->update_frame(clock);
                // get a fresh block from the fakeframepool - so as not to
                // mess up refcounting
                b = fakeargs->framepool->get();
                // and copy over the prepared framedata
                ::memcpy(b.iov_base, fakeargs->buffer, b.iov_len);
                clock = ::time(NULL);
            } else {
                clock = ::time(NULL);
                continue;
            }
        } else {
            ntimeouts = 0;
        }

    	if( outq->push(b)==false )
            break;
    }
}


// create a networkserver from the settings
runtime* net_server(runtime *rteptr) {
    RTEEXEC(*rteptr, rteptr->port>1024);
    rteptr->fd = getsok(rteptr->port, "udps", "");

    return rteptr;
}
