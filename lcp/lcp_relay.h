#include "llc_connection.h"
#include "llc_string.h"

#ifndef LCP_RELAY_H
#define LCP_RELAY_H

// The CGI/BRT handshake and LLC payload queues, restricted to local plain datagrams.
stct SRelayPeer {
	llc::SUDPConnection Connection;
	llc::apobj<llc::SUDPMessage> Sent, Send, Received;
	llc::au0_t Buffer;
	llc::au3_t Seen;
	llc::aobj<llc::string> Outgoing, Files;
	llc::string Spool;
	llc::u3_t LastPong = 0;
	std::mutex Lock;
	bool Closed = false, Pong = false;
};
stct SRelayServer {
	llc::auto_socket_close Listener;
	llc::apobj<SRelayPeer> Peers;
};
stxp llc::u1_t relayPort(bool debug) { rtrn debug ? 41998 : 41999; }
llc::err_t relayListen(SRelayServer & server, bool debug);
llc::err_t relayAccept(SRelayServer & server);
llc::err_t relayConnect(SRelayPeer & peer, bool debug);
llc::err_t relayPump(SRelayPeer & peer);
llc::err_t relayQueue(SRelayPeer & peer, llc::vcst_t message);
llc::err_t relayClose(SRelayPeer & peer);
llc::err_t relayCall(SRelayPeer & peer, bool debug, llc::vcst_t request, llc::string & response);
#endif
