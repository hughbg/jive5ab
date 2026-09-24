#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <getsok.h>
#include <dosyscall.h>
#include <evlbidebug.h>
#include <threadutil.h>

using namespace std;

// Get a socket for incoming connections.
// The returned filedescriptor is in blocking mode.
//
// You *must* specify the port/protocol. Optionally specify
// a local interface to bind to. If left empty (which is
// default) bind to all interfaces.
int getsok(unsigned short port, const string& proto, const string& local) {
    int                s;
    int                fmode;
    int                soktiep( SOCK_STREAM );
    int                reuseaddr;
    string             realproto;
    unsigned int       optlen( sizeof(reuseaddr) );
    unsigned int       slen( sizeof(struct sockaddr_in) );
    protodetails_type  protodetails;
    struct sockaddr_in src;

    DEBUG(0, "Trace getsok(unsigned short port, const string& proto, const string& local)" << endl);

    DEBUG(2, "getsok: req. server socket@" << proto
             << (local.size()?("{"+local+"}"):(""))
             << ":" << port << endl);

    // proto may encode more than just tcp or udp.
    // we really need to know the underlying protocol so get it out
    if( proto.find("udp")!=string::npos )
        realproto = "udp";
    else if( proto.find("tcp")!=string::npos )
        realproto = "tcp";
    ASSERT2_COND( realproto.size()>0,
                  SCINFO("protocol '" << proto << "' is not based on UDP or TCP") );

    // If it's UDP, we change soktiep [type of the socket] from
    // SOCK_STREAM => SOCK_DGRAM. Otherwise leave it at SOCK_STREAM.
    if( realproto=="udp" )
        soktiep = SOCK_DGRAM;

    // Get the protocolnumber for the requested protocol
    protodetails = evlbi5a::getprotobyname( realproto.c_str() );
    DEBUG(4, "getsok: got protocolnumber " << protodetails.p_proto << " for " << protodetails.p_name << endl);

    // attempt to create a socket
    ASSERT_POS( s=::socket(PF_INET, soktiep, protodetails.p_proto) );
    DEBUG(4, "getsok: got socket " << s << endl);

    // Set in blocking mode
    fmode = fcntl(s, F_GETFL);
    fmode &= ~O_NONBLOCK;
    ASSERT2_ZERO( ::fcntl(s, F_SETFL, fmode), ::close(s) );

    // Before we actually do the bind, set 'SO_REUSEADDR' to 1
    reuseaddr = 1;
    ASSERT2_ZERO( ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, optlen),
                  ::close(s) );

#ifdef SO_REUSEPORT
    // If available, create all UDP sockets with SO_REUSEPORT so we can
    // open >1 sokkits lissnin on the same port to divide the incoming
    // packet load over >1 threads
    //      https://blog.cloudflare.com/how-to-receive-a-million-packets/
    //      https://lwn.net/Articles/542629/
    if( soktiep==SOCK_DGRAM ) {
        const int    reuseport( 1 );
        unsigned int portlen( sizeof(reuseport) );
        ASSERT2_ZERO( ::setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &reuseport, portlen),
                      ::close(s) );
    }
#endif

    // Bind to local
    src.sin_family      = AF_INET;
    src.sin_port        = htons( port );
    src.sin_addr.s_addr = INADDR_ANY;

    // if 'local' not empty, attempt to bind to that address
    // First try the simple conversion, otherwise we need to do
    // a lookup. Which is to say - if it isn't a multicast address.
	// In which case the multicast will be joined rather than a bind
	// to a local ip address
    if( local.size() ) {
		struct in_addr   ip;

        // first resolve <local>
        if( inet_pton(AF_INET, local.c_str(), &ip)==-1 ) {
            int                gai_error;
            struct addrinfo    hints;
            struct addrinfo*   resultptr = 0, *rp;

            // Provide some hints
            ::memset(&hints, 0, sizeof(struct addrinfo));
            hints.ai_family   = AF_INET;       // IPv4 only at the moment
            hints.ai_socktype = soktiep;       // only the socket type we require
            hints.ai_protocol = protodetails.p_proto; // Id. for the protocol

            ASSERT2_ZERO( (gai_error=::getaddrinfo(local.c_str(), 0, &hints, &resultptr)),
                    SCINFO("[" << local << "] " << ::gai_strerror(gai_error)); ::freeaddrinfo(resultptr) );

            // Scan the results for an IPv4 address
            ip.s_addr = INADDR_ANY;
            for(rp=resultptr; rp!=0 && ip.s_addr==INADDR_ANY; rp=rp->ai_next) {
                if( rp->ai_family==AF_INET )
                    ip = ((struct sockaddr_in const*)rp->ai_addr)->sin_addr;
            }
            // don't need the list of results anymore
            ::freeaddrinfo(resultptr);
            // If we din't find one, give up
            ASSERT2_COND( ip.s_addr!=INADDR_ANY,
                    SCINFO(" - No IPv4 address found for " << local) );
        }
// If we compile with -D_POSIX_C_SOURCE -D_XOPEN_SOURCE
// we don't get support for multicast apparently. Sigh.
//#if defined(IN_MULITCAST)
        // Good. <ip> now contains the ipaddress specified in <local>
		// If multicast detected, join the group and throw up if it fails.
		if( IN_MULTICAST(ntohl(ip.s_addr)) ) {
			// ok do the MC join
            unsigned char   newttl( 30 );
			struct ip_mreq  mcjoin;

			DEBUG(1, "getsok: joining multicast group " << local << endl);

			// By the looks of the docs we do not have to do a lot more than a group-join.
			// The other options are irrelevant for us.
			// (*) We're interested in MC traffik on any interface.
			mcjoin.imr_multiaddr        = ip;
			mcjoin.imr_interface.s_addr = INADDR_ANY; // (*)
			ASSERT_ZERO( ::setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
						              &mcjoin, sizeof(mcjoin)) );

            // okay, we did connex0r to a multicast addr.
            // Possibly, failing to set the ttl is not fatal
            // but we _do_ warn the user that their data
            // may not actually arrive!
            if( ::setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL,
                        &newttl, sizeof(newttl))!=0 ) {
                DEBUG(-1, "getsok: WARN Failed to set MulticastTTL to "
                        << newttl << endl);
                DEBUG(-1, "getsok: WARN Your data may or may not arrive, "
                        << "depending on LAN or WAN" << endl);
            }
		} else {
//#endif
            src.sin_addr = ip;
			DEBUG(1, "getsok: binding to local address " << local << " " << inet_ntoa(src.sin_addr) << endl);
//#if defined(IN_MULITCAST)
		}
//#endif
    }
	// whichever local address we have - we must bind to it
	ASSERT2_ZERO( ::bind(s, (const struct sockaddr*)&src, slen),
                  SCINFO(proto << ":" << port << " [" << local << "]"); ::close(s); );

    // Ok. It's bound.
    // Now do the listen()
    if( realproto=="tcp" ) {
        DEBUG(3, "getsok: listening on interface " << local << endl);
        ASSERT2_ZERO( ::listen(s, 5), ::close(s) );
    }

    return s;
}

///////////////////////////////////////////////
/// helper stuff 
///////////////////////////////////////////////
scopedfd::scopedfd( int (*closefn)(int) /*(const std::string& proto*/):
    mFileDescriptor(-1), mCloseFn(closefn) //mProto(proto)
{}

scopedfd::scopedfd(int fd, int (*closefn)(int) /*const std::string& proto*/):
    mFileDescriptor(fd), mCloseFn(closefn) //mProto(proto)
{}

scopedfd::~scopedfd() {
    if( mFileDescriptor>=0 ) {
        DEBUG(3, "scopedfd: closing fd=" << mFileDescriptor << " (" << (mCloseFn==&::close ? "system" : "external") /*mProto*/ << ")" << endl);
        mCloseFn( mFileDescriptor );
#if 0
        if( mProto=="udt" )
            UDT::close(mFileDescriptor);
        else
            ::close(mFileDescriptor);
#endif
    }
}

