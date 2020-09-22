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
	adapter->checksum_update = false;
	adapter->checksum_sum = 0;
	adapter->checksum_del = 0;
	memset(adapter->serverdomain, 0, sizeof(adapter->serverdomain));
	memset(adapter->socket, INVALID_SOCKET, sizeof(adapter->socket));

    mobile_init(&adapter->mobile, adapter, NULL);
}

void mMobile_clear(struct mMobileAdapter* adapter) {
	adapter->timeLeach = 0;
	adapter->mobile.commands.session_begun = false;
	memset(adapter->socket, INVALID_SOCKET, sizeof(adapter->socket));

    for (int i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
		SocketClose(adapter->socket[i]);
		adapter->socket[i] = INVALID_SOCKET;
		adapter->mobile.commands.connections[i] = false;
    }

    mobile_serial_reset(&adapter->mobile);
}

void mobile_board_debug_cmd(void *user, const int send, const struct mobile_packet *packet) {
    char buffer[DEBUGCMD_BUFFERSIZE];
	size_t l;
    USER1;

    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, DEBUGCMD_BUFFERSIZE, "Command: %02X (Length: %u)\nData:\n", packet->command, packet->length);
    l = strlen(buffer);

    for (unsigned int i = 0; i < packet->length; i++) {
	    snprintf(buffer+l+(i*3), DEBUGCMD_BUFFERSIZE-l-(i*3), "\t%02x", packet->data[i]);
    }

    mLOG(MOBILEADAPTER, DEBUG, buffer);
}

void mobile_board_serial_disable(void *user) {
	mLOG(MOBILEADAPTER, DEBUG, "Serial: disabled");
}

void mobile_board_serial_enable(void *user) {
	mLOG(MOBILEADAPTER, DEBUG, "Serial: enable");
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
		mLOG(MOBILEADAPTER, WARN, "Game attempt to read an invalid config data. Offset %u size %u", offset, size);
        return false;
	}

    memcpy(adapter->config + offset, src, size);

    for (size_t i = 0; i < (size - 0x0A); i++) {
		if (memcmp(adapter->config + offset + i, "dion.ne.jp", 0x0A) == 0) {
            adapter->checksum_sum = 0;
			for (size_t m = 0; m < 0x0A; m++) {
				adapter->checksum_del += adapter->config[offset + i + m];
				adapter->config[offset + i + m] = adapter->serverdomain[m];
				adapter->checksum_sum += adapter->serverdomain[m];
            }

            // for more than one dion.ne.jp
            i += 0x0A;
            adapter->checksum_update = true;
        }
    }

    // The adapter request the last bytes (where the checksum is contained 190 and 191)
    if ((offset + size) > 0xBE && adapter->checksum_update) {
		uint16_t checksum = adapter->config[offset + size - 2] + (adapter->config[offset + size - 1] << 8);
		checksum -= adapter->checksum_del;
		checksum += adapter->checksum_sum;
		adapter->checksum_update = false;
		adapter->checksum_sum = 0;
		adapter->checksum_del = 0;
		adapter->config[offset + size - 2] = checksum & 0xFF;
		adapter->config[offset + size - 1] = checksum >> 8;
	}
    
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

    adapter->socket[conn] = sock;
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

    adapter->socket[conn] = sock;
    return true;
}

bool mobile_board_tcp_accept(void *user, unsigned conn) {
    Socket sock;
    USER2(false);
    
    mLOG(MOBILEADAPTER, INFO, "Accepting TCP connection for connection %u", conn);

    if (SocketPoll(1, &adapter->socket[conn], NULL, NULL, 1000000) > 0) {
		mLOG(MOBILEADAPTER, ERROR, "Error in pooling the socket for id %u\nNative error %d", conn, SocketError());
		return false;
    }

    sock = SocketAccept(adapter->socket[conn], NULL);
    if (SOCKET_FAILED(sock)) {
		mLOG(MOBILEADAPTER, ERROR, "Can't accept socket for id %u\nNative error %d", conn, SocketError());
        return false;	
    }
    
    SocketClose(adapter->socket[conn]);
    adapter->socket[conn] = sock;
    return true;
}

void mobile_board_tcp_disconnect(void *user, unsigned conn) {
    USER1;

    if (SOCKET_FAILED(adapter->socket[conn])) {
        SocketClose(adapter->socket[conn]);
        adapter->socket[conn] = INVALID_SOCKET;
    }

    mLOG(MOBILEADAPTER, INFO, "TCP disconnect with id %u", conn);
}

bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned size) {
    USER2(false);

    if (SOCKET_FAILED(adapter->socket[conn])) {
		mLOG(MOBILEADAPTER, ERROR, "Invalid TCP socket for id %u", conn);
		return false;
    }

    mLOG(MOBILEADAPTER, INFO, "Sending %u data to TCP connection %u", size, conn);

    return SocketSend(adapter->socket[conn], data, size) == size;
}

int mobile_board_tcp_receive(void *user, unsigned conn, void *data) {
	Socket pollSocket;
	ssize_t recvResult;
    USER2(-1);

    if (SOCKET_FAILED(adapter->socket[conn])) {
		mLOG(MOBILEADAPTER, ERROR, "Invalid TCP socket for id %u", conn);
		return -1;	
    }

    pollSocket = adapter->socket[conn];
	if (!SocketPoll(1, &pollSocket, NULL, NULL, 0)) {
		mLOG(MOBILEADAPTER, INFO, "No data received from TCP id %u", conn);
		return 0;
	}

    recvResult = SocketRecv(pollSocket, data, MOBILE_MAX_TCP_SIZE);

    if (recvResult == -1) {
		mLOG(MOBILEADAPTER, ERROR, "Error while receiving data from TCP id %u\nNative error %d", conn, SocketError());
		return recvResult;
    }

    mLOG(MOBILEADAPTER, INFO, "Received %u data from TCP id %u", recvResult, conn);
	return recvResult;
}

void mobile_board_time_latch(void *user) { 
    USER1;
    
    if (adapter->timing) {
		adapter->timeLeach = mTimingGlobalTime(adapter->timing);
    }
}

bool mobile_board_time_check_ms(void *user, const unsigned ms) {
    USER2(false);

    if (!adapter->timing) {
        return false;
    }

	return (mTimingGlobalTime(adapter->timing) - adapter->timeLeach) >
	    (int32_t)((double) ms * (1 << 21) / 1000);
}
