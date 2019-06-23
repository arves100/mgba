/*
    Copyright (C) 2019 Arves100.
    License: MPL v2
    File: mobileadaptergb.c
    Desc: Mobile Adapter GB compatibility code for mGBA
    Note: Based from printer.c


   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include <mgba/feature/mobileadaptergb.h>

static bool MobileAdapterGBInit(struct GBSIODriver* driver);
static void MobileAdapterGBDeinit(struct GBSIODriver* driver);
static void MobileAdapterGBWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t MobileAdapterGBWriteSC(struct GBSIODriver* driver, uint8_t value);
static void MobileAdapterGBProcessData(struct MobileAdapterGB* adapter);
static void MobileAdapterGBResetServerData(struct MobileAdapterGBExternalServerData* server);

void MobileAdapterGBCreate(struct MobileAdapterGB* adapter) {
	adapter->gbDriver.init = MobileAdapterGBInit;
	adapter->gbDriver.deinit = MobileAdapterGBDeinit;
	adapter->gbDriver.writeSB = MobileAdapterGBWriteSB;
	adapter->gbDriver.writeSC = MobileAdapterGBWriteSC;

	adapter->dial = NULL;
	adapter->disconnect = NULL;
	adapter->hangUp = NULL;
	adapter->login = NULL;
	adapter->logout = NULL;
	adapter->receiveDataFromServer = NULL;
	adapter->sendDataToServer = NULL;
	adapter->resolveDns = NULL;
	adapter->saveConfiguration = NULL;
	adapter->readConfiguration = NULL;
}

void MobileAdapterGBResetServerData(struct MobileAdapterGBExternalServerData* server) {
	memset(server->address, 0, 256);
	server->isConnectionOpened = false;
	server->port = 0;
}

bool MobileAdapterGBInit(struct GBSIODriver* driver) {
	struct MobileAdapterGB* adapter = (struct MobileAdapterGB*) driver;

	MobileAdapterGBResetServerData(&adapter->server);

	adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;

	adapter->buffer = NULL;
	adapter->bufferLength = 0;
	adapter->checksum = 0;
	adapter->command = 0;
	adapter->deviceType = MOBILE_ADAPTER_GB_DEVICE_PDC;

	adapter->isLineBusy = false;
	adapter->isLoggedIn = false;
	adapter->isSending = false;

	return true;
}

void MobileAdapterGBDeinit(struct GBSIODriver* driver) {
	struct MobileAdapterGB* adapter = (struct MobileAdapterGB*) driver;

	if (adapter->buffer)
		free(adapter->buffer);

	adapter->buffer = NULL;
}

static void MobileAdapterGBWriteSB(struct GBSIODriver* driver, uint8_t value) {
	struct MobileAdapterGB* adapter = (struct MobileAdapterGB*) driver;
	uint8_t returnValue = 0x00;

	switch (adapter->status) {
	case MOBILE_ADAPTER_GB_STATUS_WAITING:
		if (value == 0x99 || adapter->isSending)
			adapter->status = MOBILE_ADAPTER_GB_STATUS_PREAMBLE;

		if (adapter->isSending)
			returnValue = 0x99;

		break;
	case MOBILE_ADAPTER_GB_STATUS_PREAMBLE:
		if (value == 0x66 || adapter->isSending)
			adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_START;
		else
			adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;

		if (adapter->isSending)
			returnValue = 0x66;

		break;

	case MOBILE_ADAPTER_GB_STATUS_PACKET_START:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_01;

		if (adapter->isSending)
			returnValue = adapter->command;
		else
			adapter->command = value;

		break;

	case MOBILE_ADAPTER_GB_STATUS_PACKET_01:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_02;
		break;
	case MOBILE_ADAPTER_GB_STATUS_PACKET_02:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_LENGTH;
		break;

	case MOBILE_ADAPTER_GB_STATUS_PACKET_LENGTH:
		if (adapter->isSending) {
			if (adapter->bufferLength > 0)
				adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_BODY;
			else
				adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1;

			returnValue = adapter->bufferLength;
		} else {
			if (adapter->buffer)
				free(adapter->buffer);

			adapter->buffer = NULL;
			adapter->bufferLength = value;

			if (adapter->bufferLength > 0) {
				adapter->buffer = (uint8_t*) malloc(adapter->bufferLength);
				adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_BODY;
			} else
				adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1;
		}

		adapter->checksum = adapter->command + adapter->bufferLength;
		adapter->processedByte = 0;

		break;
	case MOBILE_ADAPTER_GB_STATUS_PACKET_BODY:
		if (!adapter->isSending) {
			adapter->buffer[adapter->processedByte] = value;
			adapter->checksum += value;
			returnValue = 0x4B;
		} else {
			returnValue = adapter->buffer[adapter->processedByte];
			adapter->checksum += returnValue;
		}

		adapter->processedByte += 1;

		if (adapter->processedByte >= adapter->bufferLength)
			adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1;

		break;
	case MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_2;

		if (adapter->isSending)
			returnValue = adapter->checksum >> 8;
		else {
			if ((adapter->checksum >> 8) != value) {
				returnValue = 0xF1;
				adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;
			}
		}

		break;
	case MOBILE_ADAPTER_GB_STATUS_CHECKSUM_2:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_DEVICE_ID;

		if (adapter->isSending)
			returnValue = adapter->checksum & 0xFF;
		else {
			if ((adapter->checksum & 0xFF) != value) {
				returnValue = 0xF1;
				adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;
			}
		}

		break;

	case MOBILE_ADAPTER_GB_STATUS_DEVICE_ID:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_STATUS_BYTE;
		returnValue = (uint8_t) adapter->deviceType;
		break;

	case MOBILE_ADAPTER_GB_STATUS_STATUS_BYTE:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;

		if (!adapter->isSending) {
			returnValue = 0x80 ^ adapter->command;
			MobileAdapterGBProcessData(adapter);
		}

		adapter->isSending = !adapter->isSending;
	}

	driver->p->pendingSB = returnValue;
}

static uint8_t MobileAdapterGBWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct MobileAdapterGB* adapter = (struct MobileAdapterGB*) driver;

	if (value & 1)
		value = value & ~1;

	return value; // We can't be the master
}

static void MobileAdapterGBProcessData(struct MobileAdapterGB* adapter) {
	uint8_t* inputData = adapter->buffer;
	uint8_t inputDataLen = adapter->bufferLength;
	uint8_t* outputData = NULL;
	uint8_t outputDataLen = 0;

	switch (adapter->command) {
	case 0x10: // Initialization command
		outputData = (uint8_t*) malloc(inputDataLen);
		outputDataLen = inputDataLen;
		memcpy(outputData, inputData, inputDataLen);
		break;

	case 0x19: { // Read configuration
		uint8_t offset = inputData[0];

		if (adapter->readConfiguration) {
			outputDataLen = inputData[1] + 1;
			outputData = (uint8_t*) malloc(outputDataLen);
			outputData[0] = offset;

			if (!adapter->readConfiguration(adapter, inputData[0], inputData[1], &outputData[1], outputDataLen)) {
				// Nor even in the py there is an error for this
				free(outputData);
				outputData = NULL;
				outputDataLen = 0;
				break;
			}
		}

		break;
	}

	case 0x1A: // Write configuration
		if (adapter->saveConfiguration)
			adapter->saveConfiguration(adapter, inputData[0], inputDataLen - 1, &inputData[1]);

		break;

	case 0x17: // Check if line is busy
		outputData = (uint8_t*) malloc(1);
		outputDataLen = 1;

		if (adapter->isLineBusy)
			outputData[0] = 0x05;
		else
			outputData[0] = 0x00;

		break;

	case 0x11: // Close connection
		if (adapter->disconnect)
			adapter->disconnect(adapter);

		MobileAdapterGBResetServerData(&adapter->server);
		adapter->isLineBusy = false;
		break;

	case 0x12: {	
		char* phone = (char*) malloc(inputDataLen + 1);
		memcpy(phone, inputData, inputDataLen);
		phone[inputDataLen] = 0;

		// Ignore PDC and CDMA DION phone
		if (strcmp(phone, "#9677") != 0 && strcmp(phone, "0077487751") != 0) {
			if (adapter->dial) {
				if (adapter->dial(adapter, phone)) {
					adapter->isLineBusy = true;
				}

				// TODO: The error packet is missing, also in py script ...
			}
		} else
			adapter->isLineBusy = true;

		break;
	}

	case 0x13: // Hung up
		if (adapter->hangUp)
			adapter->hangUp(adapter);

		adapter->isLineBusy = false;
		break;

	case 0x21: { // DION Login

		// TODO! Handle this
		adapter->isLoggedIn = true;
		break;
	}

	case 0x22: // DION Logout
		if (adapter->logout)
			adapter->logout(adapter);

		adapter->isLoggedIn = false;
		break;

	default:
		break;
	}

	if (inputData)
		free(inputData);

	adapter->buffer = outputData;
	adapter->bufferLength = outputDataLen;
}
