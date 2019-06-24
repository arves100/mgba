/*
	Mobile Adapter GB compatibility code for mGBA
	Copyright (C) 2019 Arves100.
	Released under Mozilla Public License 2.0.
 
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#ifndef MOBILE_ADAPTER_GB_H
#define MOBILE_ADAPTER_GB_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gb/sio.h>
#include <mgba/gb/interface.h>

enum MobileAdapterGBStatus {
	MOBILE_ADAPTER_GB_STATUS_WAITING,
	MOBILE_ADAPTER_GB_STATUS_PREAMBLE,
	MOBILE_ADAPTER_GB_STATUS_PACKET_START,
	MOBILE_ADAPTER_GB_STATUS_PACKET_01,
	MOBILE_ADAPTER_GB_STATUS_PACKET_02,
	MOBILE_ADAPTER_GB_STATUS_PACKET_LENGTH,
	MOBILE_ADAPTER_GB_STATUS_PACKET_BODY,
	MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1,
	MOBILE_ADAPTER_GB_STATUS_CHECKSUM_2,
	MOBILE_ADAPTER_GB_STATUS_DEVICE_ID,
	MOBILE_ADAPTER_GB_STATUS_STATUS_BYTE,
};

enum MobileAdapterGBDevice {
	MOBILE_ADAPTER_GB_DEVICE_PDC = 0x88,
	MOBILE_ADAPTER_GB_DEVICE_CDMA = 0x89,
	MOBILE_ADAPTER_GB_DEVICE_PHS = 0x8A,
	MOBILE_ADAPTER_GB_DEVICE_DDI = 0x8B,
};

struct MobileAdapterGBExternalServerData {
	char address[16];
	int port;
	bool isConnectionOpened;
};

struct MobileAdapterGB {
	struct GBSIODriver gbDriver;

	uint8_t* buffer;
	uint8_t bufferLength;
	uint8_t processedByte;
	uint16_t checksum;
	uint8_t command;

	enum MobileAdapterGBStatus status;
	enum MobileAdapterGBDevice deviceType;

	bool isSending;
	bool isLineBusy;
	bool isLoggedIn;

	struct MobileAdapterGBExternalServerData server;

	char internalServerBuffer[254];

	bool (*dial)(struct MobileAdapterGB*, const char*);
	void (*hangUp)(struct MobileAdapterGB*);
	bool (*connect)(struct MobileAdapterGB*);
	void (*disconnect)(struct MobileAdapterGB*);
	char* (*resolveDns)(struct MobileAdapterGB*, const char*);
	unsigned char* (*receiveDataFromServer)(struct MobileAdapterGB*, uint8_t*);
	const bool (*sendDataToServer)(struct MobileAdapterGB*, uint8_t, unsigned char*);
	bool (*login)(struct MobileAdapterGB*, uint8_t, const char*, uint8_t, const char*);
	void (*logout)(struct MobileAdapterGB*);
	bool (*readConfiguration)(struct MobileAdapterGB*, uint8_t, uint8_t, unsigned char*);
	void (*saveConfiguration)(struct MobileAdapterGB*, uint8_t, uint8_t, const unsigned char*);
};

void MobileAdapterGBCreate(struct MobileAdapterGB*);

CXX_GUARD_END

#endif
