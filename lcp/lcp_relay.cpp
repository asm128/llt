#include "lcp_relay.h"
#include "llc_chrono.h"
#include "llc_sync.h"
#include "llc_file.h"
#include "llc_path.h"
#include "llc_string_compose.h"
#ifdef LLC_WINDOWS
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#endif

using namespace llc;

stxp vcst_t FilePacket = "\x1eLCPFILE:";
#ifdef LLC_WINDOWS
sttc err_t relaySpool(SRelayPeer & peer, bool debug) {
	char executable[32768] = {};
	cnst DWORD count = GetModuleFileNameA(nullptr, executable, (DWORD)size(executable));
	if_true_fe(!count || count >= size(executable));
	vcst_t folder;
	if_fail_fe(pathDirectory({executable, count}, folder));
	string relative;
	if_fail_fe(append_strings(relative, folder, "../.lls-relay/", debug ? vcst_t{"Debug"} : vcst_t{"Release"}));
	if_fail_fe(peer.Spool.clear());
	rtrn pathAbsolute(relative, peer.Spool);
}

sttc err_t relayFileWrite(SRelayPeer & peer, vcst_t message, string & packet) {
	if_fail_fe(pathCreate(peer.Spool));
	cnst DWORD attributes = GetFileAttributesA(peer.Spool.begin());
	if_true_fe(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT));
	GUID id = {};
	if_true_fe(FAILED(CoCreateGuid(&id)));
	char name[33] = {};
	cnst u0_t * bytes = (cnst u0_t*)&id;
	cnst char hex[] = "0123456789abcdef";
	for(u2_t i = 0; i < sizeof(id); ++i) { name[i * 2] = hex[bytes[i] >> 4]; name[i * 2 + 1] = hex[bytes[i] & 15]; }
	string path;
	if_fail_fe(append_strings(path, peer.Spool, '/', vcst_t{name, 32}, ".tmp"));
	if_fail_fe(peer.Files.push_back(path));
	if_fail_fe(fileFromMemory(path, message));
	rtrn append_strings(packet, FilePacket, path);
}

sttc err_t relayFileRead(SRelayPeer & peer, au0_t & payload) {
	vcst_t packet = {(cnst char*)payload.begin(), payload.size()};
	if(packet.find(FilePacket) != 0) rtrn 0;
	vcst_t path;
	if_fail_fe(packet.slice(path, FilePacket.size()));
	string prefix;
	if_fail_fe(append_strings(prefix, peer.Spool, '/'));
	if_true_fe(path.find(prefix) != 0 || path.size() != prefix.size() + 36);
	for(u2_t i = prefix.size(); i < path.size() - 4; ++i) {
		if_true_fe(!((path[i] >= '0' && path[i] <= '9') || (path[i] >= 'a' && path[i] <= 'f')));
	}
	if_true_fe(path[path.size()-4] != '.' || path[path.size()-3] != 't' || path[path.size()-2] != 'm' || path[path.size()-1] != 'p');
	string filename = path;
	cnst DWORD attributes = GetFileAttributesA(filename.begin());
	if_true_fe(attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)));
	au0_t contents;
	if_fail_fe(fileToMemory(path, contents));
	if_fail_fe(fileDelete(path));
	payload = contents;
	rtrn 0;
}

sttc err_t relaySocket(auto_socket_close & output, u1_t port) {
	output.close();
	output.Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if_true_fe(output == INVALID_SOCKET);
	sockaddr_in local = {};
	if_fail_fe(tcpipAddressToSockaddr(127, 0, 0, 1, port, local));
	BOOL exclusive = TRUE;
	if_true_fe(setsockopt(output, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (cnst char*)&exclusive, sizeof(exclusive)));
	if_true_fe(bind(output, (sockaddr*)&local, sizeof(local)));
	u_long nonblocking = 1;
	if_true_fe(ioctlsocket(output, FIONBIO, &nonblocking));
	rtrn 0;
}
sttc err_t relayCommand(SUDPConnection & connection, SUDPCommand command) {
	sockaddr_in remote = {};
	if_fail_fe(tcpipAddressToSockaddr(connection.Address, remote));
	rtrn sendto(connection.Socket, (cnst char*)&command, sizeof(command), 0, (sockaddr*)&remote, sizeof(remote)) == sizeof(command) ? 0 : -1;
}
#endif

err_t relayClose(SRelayPeer & peer) {
	llc::lock_guard guard(peer.Lock);
#ifdef LLC_WINDOWS
	if(peer.Connection.Socket != INVALID_SOCKET && peer.Connection.State == UDP_CONNECTION_STATE_IDLE) {
		SUDPCommand command = {};
		command.Command = ENDPOINT_COMMAND_DISCONNECT;
		if_fail_e(relayCommand(peer.Connection, command));
	}
#endif
	peer.Closed = true;
	peer.Connection.State = UDP_CONNECTION_STATE_DISCONNECTED;
	peer.Connection.Socket.close();
	if_fail_fe(peer.Outgoing.clear());
	for(cnst string & path : peer.Files) {
#ifdef LLC_WINDOWS
		if(GetFileAttributesA(path.begin()) != INVALID_FILE_ATTRIBUTES) { if_fail_e(fileDelete(path)); }
#endif
	}
	if_fail_fe(peer.Files.clear());
	if_fail_fe(clear(peer.Connection.Queue.Send, peer.Connection.Queue.Sent, peer.Connection.Queue.Received, peer.Sent, peer.Send, peer.Received, peer.Seen));
	peer.Pong = false;
	rtrn 0;
}

err_t relayQueue(SRelayPeer & peer, vcst_t message) {
	llc::lock_guard guard(peer.Lock);
	if(peer.Closed) rtrn 0; // The job remains in the dashboard; never deliver it to another connection.
	string packet;
#ifdef LLC_WINDOWS
	if(message.size() > UDP_PAYLOAD_SIZE_LIMIT) {
		if_fail_fe(relayFileWrite(peer, message, packet));
		message = packet;
	}
#endif
	if_fail_fe(peer.Outgoing.push_back(message));
	rtrn 0;
}

err_t relayListen(SRelayServer & server, bool debug) {
#ifdef LLC_WINDOWS
	rtrn relaySocket(server.Listener, relayPort(debug));
#else
	(void)server; (void)debug;
	rtrn -1;
#endif
}

err_t relayAccept(SRelayServer & server) {
#ifdef LLC_WINDOWS
	for(u2_t count = 0; count < 16; ++count) {
		SUDPCommand command = {};
		sockaddr_in remote = {};
		int remoteSize = sizeof(remote);
		cnst int received = recvfrom(server.Listener, (char*)&command, sizeof(command), 0, (sockaddr*)&remote, &remoteSize);
		if(received < 0) {
			cnst int error = WSAGetLastError();
			if(error == WSAEWOULDBLOCK) rtrn 0;
			if(error == WSAEMSGSIZE) continue;
			rtrn -1;
		}
		if(received != sizeof(command) || remote.sin_addr.s_addr != htonl(INADDR_LOOPBACK)
			|| command.Command != ENDPOINT_COMMAND_CONNECT || command.Type != ENDPOINT_COMMAND_TYPE_REQUEST
			|| command.Packed || command.Encrypted || command.Compressed || command.Multipart || command.Resend)
			continue;
		if(server.Peers.size() >= 128) continue;
		pobj<SRelayPeer> peer;
		SIPv4End listener;
		if_fail_fe(tcpipAddress(server.Listener, listener));
		if_fail_fe(relaySocket(peer->Connection.Socket, 0));
		if_fail_fe(relaySpool(*peer, listener.Port == relayPort(true)));
		if_fail_fe(tcpipAddressFromSockaddr(remote, peer->Connection.Address));
		peer->Connection.State = UDP_CONNECTION_STATE_HANDSHAKE;
		peer->Connection.FirstPing = peer->Connection.LastPing = timeCurrentInUs();
		peer->Connection.KeyPing = 1; // Plain local mode does not use the historical encryption clock recovery.
		command.Type = ENDPOINT_COMMAND_TYPE_RESPONSE;
		if_fail_fe(relayCommand(peer->Connection, command));
		if_fail_fe(server.Peers.push_back(peer));
	}
	rtrn 0;
#else
	(void)server;
	rtrn -1;
#endif
}

err_t relayPump(SRelayPeer & peer) {
#ifdef LLC_WINDOWS
	SUDPConnection & connection = peer.Connection;
	if(peer.Closed || connection.Socket == INVALID_SOCKET) rtrn -1;
	if_fail_fe(peer.Buffer.resize(UDP_PAYLOAD_SIZE_LIMIT + sizeof(SUDPPayloadHeader) + 1));
	for(u2_t count = 0; count < 32; ++count) {
		sockaddr_in remote = {};
		int remoteSize = sizeof(remote);
		cnst int received = recvfrom(connection.Socket, (char*)peer.Buffer.begin(), (int)peer.Buffer.size(), MSG_PEEK, (sockaddr*)&remote, &remoteSize);
		if(received < 0) {
			cnst int error = WSAGetLastError();
			if(error == WSAEWOULDBLOCK) break;
			if(error != WSAEMSGSIZE) rtrn -1;
		}
		SIPv4End source;
		if_fail_fe(tcpipAddressFromSockaddr(remote, source));
		bool valid = received >= (int)sizeof(SUDPCommand) && source == connection.Address;
		SUDPCommand command = valid ? *(SUDPCommand*)peer.Buffer.begin() : SUDPCommand{};
		valid &= !command.Encrypted && !command.Compressed && !command.Multipart && !command.Resend;
		bool duplicate = false;
		if(valid && command.Command == ENDPOINT_COMMAND_PAYLOAD) {
			valid = !command.Packed && received >= (int)sizeof(SUDPPayloadHeader) && connection.State == UDP_CONNECTION_STATE_IDLE;
			if(valid) {
				cnst SUDPPayloadHeader & header = *(cnst SUDPPayloadHeader*)peer.Buffer.begin();
				valid = header.Size <= UDP_PAYLOAD_SIZE_LIMIT && (u2_t)received == sizeof(header) + header.Size;
				if(valid && command.Type == ENDPOINT_COMMAND_TYPE_REQUEST) {
					duplicate = peer.Seen.find(header.MessageId) >= 0;
					if(!duplicate) { if_fail_fe(peer.Seen.push_back(header.MessageId)); }
				}
			}
		}
		else if(valid)
			valid = received == sizeof(SUDPCommand);
		if(valid && !duplicate) {
			connection.LastPing = timeCurrentInUs();
			if(command.Command == ENDPOINT_COMMAND_NOOP) {
				if(command.Type == ENDPOINT_COMMAND_TYPE_REQUEST) {
					command.Type = ENDPOINT_COMMAND_TYPE_RESPONSE;
					if_fail_fe(relayCommand(connection, command));
				}
				else { peer.Pong = true; peer.LastPong = timeCurrentInUs(); }
			}
			else {
				au0_t payload;
				if_fail_fe(connectionHandleCommand(connection, command, payload));
			}
		}
		if(connection.Socket == INVALID_SOCKET) { if_fail_fe(relayClose(peer)); rtrn -1; }
		// The recovered handler peeks. Consume exactly this datagram, even when rejected.
		recvfrom(connection.Socket, (char*)peer.Buffer.begin(), (int)peer.Buffer.size(), 0, 0, 0);
	}
	{
		llc::lock_guard guard(peer.Lock);
		for(cnst string & message : peer.Outgoing) {
			if_fail_fe(connectionPushData(connection, connection.Queue, vcu0_t{(cnst u0_t*)message.begin(), message.size()}, false, false, 0));
		}
		if_fail_fe(peer.Outgoing.clear());
		for(u2_t i = 0; i < peer.Files.size(); ++i)
			if(GetFileAttributesA(peer.Files[i].begin()) == INVALID_FILE_ATTRIBUTES) { if_fail_fe(peer.Files.remove_unordered(i--)); }
	}
	if_fail_fe(connectionPayloadCollect(connection, peer.Received));
	for(auto & message : peer.Received) { if_fail_fe(relayFileRead(peer, message->Payload)); }
	if_fail_fe(connectionSendQueue(connection, peer.Sent, peer.Send, false));
	rtrn 0;
#else
	(void)peer;
	rtrn -1;
#endif
}

err_t relayConnect(SRelayPeer & peer, bool debug) {
#ifdef LLC_WINDOWS
	if_fail_fe(relaySpool(peer, debug));
	if_fail_fe(relaySocket(peer.Connection.Socket, 0));
	peer.Closed = false;
	peer.Connection.State = UDP_CONNECTION_STATE_HANDSHAKE;
	peer.Connection.Address = {SIPv4{127, 0, 0, 1}, relayPort(debug)};
	SUDPCommand command = {};
	command.Command = ENDPOINT_COMMAND_CONNECT;
	if_fail_fe(relayCommand(peer.Connection, command));
	cnst s3_t deadline = timeCurrentInUs() + 2000000;
	while(timeCurrentInUs() < deadline) {
		sockaddr_in remote = {};
		int remoteSize = sizeof(remote);
		SUDPCommand response = {};
		cnst int received = recvfrom(peer.Connection.Socket, (char*)&response, sizeof(response), 0, (sockaddr*)&remote, &remoteSize);
		if(received == sizeof(response) && remote.sin_addr.s_addr == htonl(INADDR_LOOPBACK)
			&& response.Command == ENDPOINT_COMMAND_CONNECT && response.Type == ENDPOINT_COMMAND_TYPE_RESPONSE
			&& !response.Packed && !response.Encrypted && !response.Compressed && !response.Multipart) {
			if_fail_fe(tcpipAddressFromSockaddr(remote, peer.Connection.Address));
			command.Packed = 1;
			if_fail_fe(relayCommand(peer.Connection, command));
			peer.Connection.State = UDP_CONNECTION_STATE_IDLE;
			peer.Connection.FirstPing = peer.Connection.LastPing = timeCurrentInUs();
			peer.Connection.KeyPing = 1;
			rtrn 0;
		}
		sleep(1);
	}
	if_fail_fe(relayClose(peer));
	rtrn -1;
#else
	(void)peer; (void)debug;
	rtrn -1;
#endif
}

#ifdef LLC_WINDOWS
sttc err_t relayProbe(SRelayPeer & peer) {
	peer.Pong = false;
	SUDPCommand ping = {};
	if_fail_fe(relayCommand(peer.Connection, ping));
	cnst s3_t deadline = timeCurrentInUs() + 2000000;
	while(timeCurrentInUs() < deadline) {
		if_fail_fe(relayPump(peer));
		if(peer.Pong) rtrn 0;
		sleep(1);
	}
	rtrn -1;
}
#endif

err_t relayCall(SRelayPeer & peer, bool debug, vcst_t request, string & response, u2_t timeoutMs) {
#ifdef LLC_WINDOWS
	if(peer.Connection.State == UDP_CONNECTION_STATE_IDLE && failed(relayProbe(peer))) {
		if_fail_fe(relayClose(peer));
	}
	if(peer.Connection.State != UDP_CONNECTION_STATE_IDLE) {
		if_fail_fe(relayConnect(peer, debug));
		if_fail_fe(relayProbe(peer));
	}
	peer.LastPong = timeCurrentInUs();
	if_fail_fe(relayQueue(peer, request));
	cnst s3_t deadline = timeCurrentInUs() + (s3_t)timeoutMs * 1000;
	u3_t heartbeat = 0;
	while(timeCurrentInUs() < deadline) {
		if_fail_fe(relayPump(peer));
		if(peer.Received.size()) {
			cnst auto & payload = peer.Received[0]->Payload;
			if_fail_fe(response.append({(cnst char*)payload.begin(), payload.size()}));
			if_fail_fe(peer.Received.remove(0));
			rtrn 0;
		}
		cnst u3_t now = timeCurrentInUs();
		if(now >= heartbeat) {
			SUDPCommand ping = {};
			if_fail_fe(relayCommand(peer.Connection, ping));
			heartbeat = now + 1000000;
		}
		if(now - peer.LastPong > 5000000) break;
		sleep(1);
	}
	if_fail_fe(relayClose(peer));
	rtrn -1; // Delivery/completion is unknown. Never retry the application command.
#else
	(void)peer; (void)debug; (void)request; (void)response; (void)timeoutMs;
	rtrn -1;
#endif
}
