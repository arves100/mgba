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

#define USER1 struct mMobileAdapter* adapter = (struct mMobileAdapter*)user; if (!adapter) return;
#define USER2(ret) struct mMobileAdapter* adapter = (struct mMobileAdapter*)user; if (!adapter) return ret;
#define DEBUGCMD_BUFFERSIZE (MOBILE_MAX_DATA_SIZE*3)+100

void mMobile_init(struct mMobileAdapter* adapter) { 
    mobile_init(&adapter->mobile, adapter, NULL);
    memset(adapter->socket, INVALID_SOCKET, sizeof(adapter->socket));
    adapter->timeLeach = 0;
    adapter->timing = NULL;
}

void mMobile_clear(struct mMobileAdapter* adapter) {
	adapter->timeLeach = 0;
	memset(adapter->socket, INVALID_SOCKET, sizeof(adapter->socket));

    for (int i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
		SocketClose(adapter->socket[i]);
		adapter->socket[i] = INVALID_SOCKET;
    }
}

void mobile_board_debug_cmd(void *user, const int send, const struct mobile_packet *packet) {
    char buffer[DEBUGCMD_BUFFERSIZE];
    size_t l;
    USER1;

    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, DEBUGCMD_BUFFERSIZE, "Command: %02X (Length: %u)\nData:\n", packet->command, packet->length);
    l = strlen(buffer) + 1;

    for (unsigned int i = 0; i < packet->length; i++) {
	    snprintf(buffer+l+i, DEBUGCMD_BUFFERSIZE-l-i, "\t%d", hexDigit(packet->data[i]));
    }

    mLOG(MOBILEADAPTER, DEBUG, buffer);
}

void mobile_board_serial_disable(void *user) {
}

void mobile_board_serial_enable(void *user) {
}

bool mobile_board_config_read(void *user, void *dest, const uintptr_t offset, const size_t size) {
    USER2(false);

    if (offset + size > 192)
        return false;

    memcpy(dest, adapter->config + offset, size);
    return true;
}

bool mobile_board_config_write(void *user, const void *src, const uintptr_t offset, const size_t size) { 
    USER2(false);

    if (offset + size > 192)
        return false;

    memcpy(adapter->config + offset, src, size);
    return true;
}

bool mobile_board_tcp_connect(void *user, unsigned conn, const unsigned char *host, const unsigned port) { 
    struct Address addr;
    Socket sock;
    USER2(false);
    
    addr.version = IPV4;
	addr.ipv4 = 0;

	for (int i = 0; i < 4; ++i) {
		addr.ipv4 <<= 8;
		addr.ipv4 += host[i];
	}

    sock = SocketConnectTCP(port, &addr);
    if (sock == INVALID_SOCKET)
        return false;

    adapter->socket[conn] = sock;
    return true;
}

bool mobile_board_tcp_listen(void *user, unsigned conn, const unsigned port) {
    Socket sock;
    USER2(false);

    sock = SocketOpenTCP(port, NULL);
    if (sock == INVALID_SOCKET)
        return false;

    if (SocketListen(sock, 1024) == -1) {
	    SocketClose(sock);
        return false;
    }

    adapter->socket[conn] = sock;
    return true;
}

bool mobile_board_tcp_accept(void *user, unsigned conn) {
    Socket sock;
    USER2(false);
    
    if (SocketPoll(1, &adapter->socket[conn], NULL, NULL, 1000000) > 0)
        return false;

    sock = SocketAccept(adapter->socket[conn], NULL);
    if (sock == INVALID_SOCKET)
        return false;
    
    SocketClose(adapter->socket[conn]);
    adapter->socket[conn] = sock;
    return true;
}

void mobile_board_tcp_disconnect(void *user, unsigned conn) {
    USER1;

    if (adapter->socket[conn] != INVALID_SOCKET) {
        SocketClose(adapter->socket[conn]);
        adapter->socket[conn] = INVALID_SOCKET;
    }
}

bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned size) {
    USER2(false);
    if (adapter->socket[conn] == INVALID_SOCKET)
        return false;

    return SocketSend(adapter->socket[conn], data, size) == size;
}

int mobile_board_tcp_receive(void *user, unsigned conn, void *data) {
    USER2(-1);

    if (adapter->socket[conn] == INVALID_SOCKET)
        return -1;

    if (!SocketPoll(1, &adapter->socket[conn], NULL, NULL, 0))
        return 0;

    return SocketRecv(adapter->socket[conn], data, MOBILE_MAX_DATA_SIZE - 1);
}

unsigned char mobile_board_dns_ip[4]; // workaround for libmobile

bool mobile_board_dns_query(unsigned char *ip, const char *host, const unsigned char  *dns1, const unsigned char *dns2) {
    memcpy(ip, mobile_board_dns_ip, 4);
    return true;
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

    return false;
    //return (mTimingCurrentTime(adapter->timing) - adapter->timeLeach) > (int32_t)((double)ms * (1 << 21) / 1000); // ¡× it's not working?
}
