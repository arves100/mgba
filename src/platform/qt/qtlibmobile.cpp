/*
 * libmobile compatibility code for Qt platform
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifdef BUILD_LIBMOBILE
#include "qtlibmobile.h"

#include <mgba/core/log.h>
#include <mgba/feature/libmobile.h>

#include <QFile>
#include <QFileInfo>

mLOG_DEFINE_CATEGORY(QTADAPTERGB, "QtAdapterGB", "platform.qtadaptergb");

#define LIBMOBILE_CONFIG_FILE "mobileadapter.dat"

static unsigned long millis();
static void makeConfigFile() {
	QFileInfo info(LIBMOBILE_CONFIG_FILE);

	if (!info.exists()) {
		QFile file(LIBMOBILE_CONFIG_FILE);

		if (file.open(QIODevice::WriteOnly)) {
			char data[192];
			memset(data, 0, sizeof(data));
			file.write(data, 192);
			file.close();

			mLOG(QTADAPTERGB, INFO, "Created empty mobile adapter config file");
		} else {
			mLOG(QTADAPTERGB, ERROR, "Cannot create mobile adapter config file");
		}
	}
}

// Serial as not required, we're not an Arduino
void mobile_board_serial_disable(void*) {}
void mobile_board_serial_enable(void*) {}

void mobile_board_debug_cmd(void*, const int, const struct mobile_packet* packet) {
	mLOG(QTADAPTERGB, DEBUG, "Mobile board command %u", (unsigned int) packet->command);
}

bool mobile_board_config_read(void* user, void* dest, const uintptr_t offset, const size_t size) {
	makeConfigFile();

	QFile config(LIBMOBILE_CONFIG_FILE);

	if (!config.open(QIODevice::ReadOnly)) {
		mLOG(QTADAPTERGB, ERROR, "Cannot open mobile adapter config file");
		return false;
	}

	config.seek(offset);
	qint64 result = config.read((char*) dest, size);
	config.close();

	return result != -1;
}

bool mobile_board_config_write(void* user, const void* src, const uintptr_t offset, const size_t size) {
	makeConfigFile();

	QFile config(LIBMOBILE_CONFIG_FILE);

	if (!config.open(QIODevice::WriteOnly)) {
		mLOG(QTADAPTERGB, ERROR, "Cannot open mobile adapter config file");
		return false;
	}

	config.seek(offset);
	qint64 result = config.write((const char*) src, size);
	config.close();

	return result != -1;
}

void mobile_board_time_latch(void* user) {
	QGBA::QMobileAdapter* adapter = (QGBA::QMobileAdapter*) user;
	adapter->millis_latch = clock();
}

bool mobile_board_time_check_ms(void* user, const unsigned ms) {
	QGBA::QMobileAdapter* adapter = (QGBA::QMobileAdapter*) user;
	//return (unsigned long)(test - adapter->millis_latch) > (unsigned long) ms;
	return false;
}

bool mobile_board_tcp_connect(void* user, unsigned conn, const unsigned char* host, const unsigned port) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp connect");
	return false;
}

bool mobile_board_tcp_listen(void* user, unsigned conn, const unsigned port) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp listen");
	return false;
}

bool mobile_board_tcp_accept(void* user, unsigned conn) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp accept");
	return false;
}

void mobile_board_tcp_disconnect(void* user, unsigned conn) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp disconnect");
}

bool mobile_board_tcp_send(void* user, unsigned conn, const void* data, const unsigned size) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp send");
	return false;
}

int mobile_board_tcp_receive(void* user, unsigned conn, void* data) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp receive");
	return -10;
}

bool mobile_board_dns_query(unsigned char* ip, const char* host, const unsigned char* dns1, const unsigned char* dns2) {
	mLOG(QTADAPTERGB, DEBUG, "Triggered tcp dns query");
	memset(ip, 0, 4);
	return false;
}

#endif
