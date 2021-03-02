/*
* libmobile implementation for mGBA Core
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <mgba/feature/mobile.h>

#include <mgba-util/string.h>

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(MOBILEADAPTER);
mLOG_DEFINE_CATEGORY(MOBILEADAPTER, "Mobile Adapter", "feature.mobileadapter");

#define USER1 struct mMobileAdapter* adapter = (struct mMobileAdapter*)user; if (!adapter) { mLOG(MOBILEADAPTER, ERROR, "adapter == nullptr?"); return; }
#define USER2(ret) struct mMobileAdapter* adapter = (struct mMobileAdapter*)user; if (!adapter) { mLOG(MOBILEADAPTER, ERROR, "adapter == nullptr?"); return ret; }
#define DEBUGCMD_BUFFERSIZE 1024

void mMobile_init(struct mMobileAdapter* adapter) { 
	adapter->timeLeach = 0;
	adapter->timing = NULL;
	adapter->frequency = 0;
#if 0
	adapter->checksum_update = false;
	adapter->checksum_sum = 0;
	adapter->checksum_del = 0;
	memset(adapter->serverdomain, 0, sizeof(adapter->serverdomain));
#endif
	memset(adapter->connection, INVALID_SOCKET, sizeof(adapter->connection));

    mobile_init(&adapter->mobile, adapter, NULL);
}

void mMobile_clear(struct mMobileAdapter* adapter) {
	adapter->timeLeach = 0;
	adapter->mobile.commands.session_begun = false;
	memset(adapter->connection, INVALID_SOCKET, sizeof(adapter->connection));

    for (int i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
		SocketClose(adapter->connection[i].socket);
		adapter->connection[i].socket = INVALID_SOCKET;
		adapter->mobile.commands.connections[i] = false;
    }

    mobile_serial_reset(&adapter->mobile);
}

void mobile_board_debug_cmd(void *user, const int send, const struct mobile_packet *packet) {
    char buffer[DEBUGCMD_BUFFERSIZE];
	size_t l;
    USER1;

    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, DEBUGCMD_BUFFERSIZE, "Command: %02X (Length: %u) (%s)\nData:\n", packet->command, packet->length, send > 0 ? "SEND" : "RECV");
    l = strlen(buffer);

    for (unsigned int i = 0; i < packet->length; i++) {
	    snprintf(buffer+l+(i*3), DEBUGCMD_BUFFERSIZE-l-(i*3), "\t%02x", packet->data[i]);
    }

    mLOG(MOBILEADAPTER, DEBUG, buffer);
}

void mobile_board_serial_disable(void *user) {
}

void mobile_board_serial_enable(void *user) {
}

bool mobile_board_config_read(void *user, void *dest, const uintptr_t offset, const size_t size) {
	USER2(false);

    if (offset + size > 192) {
		mLOG(MOBILEADAPTER, WARN, "Game attempt to read an invalid config data. Offset %u size %u", offset, size);
		return false;
	}

    memcpy(dest, adapter->config + offset, size);
    return true;
}

bool mobile_board_config_write(void *user, const void *src, const uintptr_t offset, const size_t size) { 
    USER2(false);

    if (offset + size > 192) {
		mLOG(MOBILEADAPTER, WARN, "Game attempt to read an invalid config data. Offset %lu size %zu", offset, size);
        return false;
	}

    //memcpy(adapter->config + offset, src, size);

    return true;
}

bool mobile_board_tcp_connect(void *user, unsigned conn, const unsigned char *host, const unsigned port) { 
    struct Address addr;
    Socket sock;
    USER2(false);

    addr.version = IPV4;
	addr.ipv4 = adapter->serverip;

    mLOG(MOBILEADAPTER, INFO, "Connect to TCP:%d.%d.%d.%d:%d", host[0], host[1], host[2], host[3], port);

    sock = SocketConnectTCP(port, &addr);

    if (SOCKET_FAILED(sock)) {
		mLOG(MOBILEADAPTER, ERROR, "Can't connect to TCP:%d.%d.%d.%d:%d. Native error %d", host[0], host[1], host[2],
		     host[3], port, SocketError());

        return false;
	}

    adapter->connection[conn].socket = sock;

    return true;
}

bool mobile_board_tcp_listen(void *user, unsigned conn, const unsigned port) {
    Socket sock;
    USER2(false);

    mLOG(MOBILEADAPTER, INFO, "Start TCP listen for connection %u on port %d", conn, port);
   
    sock = SocketOpenTCP(port, NULL);

    if (SOCKET_FAILED(sock)) {
		mLOG(MOBILEADAPTER, ERROR, "Can't open socket to TCP port %d with id %u\nNative error %d", port, conn, SocketError());
		return false;
	}

    if (SOCKET_FAILED(SocketListen(sock, 1024))) {
		mLOG(MOBILEADAPTER, ERROR, "Can't listen socket to TCP port %d with id %u\nNative error %d", port, conn, SocketError());
        SocketClose(sock);
        return false;
    }

    adapter->connection[conn].socket = sock;
    return true;
}

bool mobile_board_tcp_accept(void *user, unsigned conn) {
    Socket sock;
    USER2(false);
    
    mLOG(MOBILEADAPTER, INFO, "Accepting TCP connection for connection %u", conn);

    if (SocketPoll(1, &adapter->connection[conn].socket, NULL, NULL, 1000000) > 0) {
		mLOG(MOBILEADAPTER, ERROR, "Error in pooling the socket for id %u\nNative error %d", conn, SocketError());
		return false;
    }

    sock = SocketAccept(adapter->connection[conn].socket, NULL);
    if (SOCKET_FAILED(sock)) {
		mLOG(MOBILEADAPTER, ERROR, "Can't accept socket for id %u\nNative error %d", conn, SocketError());
        return false;	
    }
    
    SocketClose(adapter->connection[conn].socket);
    adapter->connection[conn].socket = sock;
    return true;
}

void mobile_board_tcp_disconnect(void *user, unsigned conn) {
    USER1;

    if (SOCKET_FAILED(adapter->connection[conn].socket)) {
        SocketClose(adapter->connection[conn].socket);
        adapter->connection[conn].socket = INVALID_SOCKET;
    }

    mLOG(MOBILEADAPTER, INFO, "TCP disconnect with id %u", conn);
}

bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned size) {
    USER2(false);

    if (SOCKET_FAILED(adapter->connection[conn].socket)) {
		mLOG(MOBILEADAPTER, ERROR, "Invalid TCP socket for id %u", conn);
		return false;
    }

    mLOG(MOBILEADAPTER, INFO, "Sending %u data to TCP connection %u", size, conn);

	if (size == 0)
		return true;

    if (SocketSend(adapter->connection[conn].socket, data, size) == -1) {
        mLOG(MOBILEADAPTER, ERROR, "Error while sending data to TCP id %u\nNative error %d", conn, SocketError());
        return false;
	}

	return true;
}

int mobile_board_tcp_receive(void *user, unsigned conn, void *data) {
	Socket reads = 0, errors = 0;
	ssize_t recvResult;
    USER2(-1);

    if (SOCKET_FAILED(adapter->connection[conn].socket)) {
		mLOG(MOBILEADAPTER, ERROR, "Invalid TCP socket for id %u", conn);
		return -1;	
    }

    recvResult = SocketPoll(1, &reads, NULL, &errors, 0);

    if (recvResult == 0) {
        mLOG(MOBILEADAPTER, INFO, "No data received from TCP id %u", conn);
        return 0;
    }

    if (recvResult == -1)
    {
        mLOG(MOBILEADAPTER, ERROR, "Error while pooling data from TCP id %u\nNative error %d", conn, SocketError());
        return -1;
    }

    if (errors == adapter->connection[conn].socket)
    {
        mLOG(MOBILEADAPTER, INFO, "Disconnected from server");
        return -1; // Disconnected
    }

    recvResult = SocketRecv(adapter->connection[conn].socket, data, MOBILE_MAX_TCP_SIZE);

    if (recvResult == -1) {
		mLOG(MOBILEADAPTER, ERROR, "Error while receiving data from TCP id %u\nNative error %d", conn, SocketError());
		return -1;
    }

    mLOG(MOBILEADAPTER, INFO, "Received %zu data from TCP id %u", recvResult, conn);
	return recvResult;
}

void mobile_board_time_latch(void *user) { 
    USER1;
    
    if (adapter->timing) {
		adapter->timeLeach = mTimingCurrentTime(adapter->timing);
    }
}

bool mobile_board_time_check_ms(void *user, const unsigned ms) {
    USER2(false);

    if (!adapter->timing) {
        return false;
    }

    int32_t a = mTimingCurrentTime(adapter->timing);
    int32_t b = a - adapter->timeLeach;
    float c = b / adapter->frequency;
    return (c * 100) > ms;
}
