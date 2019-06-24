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

#include <mgba/internal/gba/io.h>

mLOG_DEFINE_CATEGORY(CGBADAPTER, "Mobile Adapter GB", "feature.mobileadaptergb");

// Common functions
static void    MobileAdapter_ProcessData(struct MobileAdapterGB* adapter);
static void    MobileAdapter_ResetServerData(struct MobileAdapterGBExternalServerData* server);
static void    MobileAdapter_FakeServer(struct MobileAdapterGB* adapter, uint8_t* length, uint8_t** data, bool receiving);
static void    MobileAdapter_Init(struct MobileAdapterGB* adapter);
static uint8_t MobileAdapter_HandleByte(struct MobileAdapterGB* adapter, uint8_t value);
static void    MobileAdapter_Deinit(struct MobileAdapterGB* adapter);

// GB SIO DRIVER
static bool    MobileAdapter_GB_Init(struct GBSIODriver* driver);
static void    MobileAdapter_GB_Deinit(struct GBSIODriver* driver);
static void    MobileAdapter_GB_WriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t MobileAdapter_GB_WriteSC(struct GBSIODriver* driver, uint8_t value);

// GBA SIO DRIVER
static bool      MobileAdapter_AGB_Init(struct GBASIODriver* driver);
static void      MobileAdapter_AGB_Deinit(struct GBASIODriver* driver);
static uint16_t  MobileAdapter_AGB_WriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);


void MobileAdapterGBCreate(struct MobileAdapterGB* adapter) {
	// GameBoy Color driver init
	adapter->gbDriver.init = MobileAdapter_GB_Init;
	adapter->gbDriver.deinit = MobileAdapter_GB_Deinit;
	adapter->gbDriver.writeSB = MobileAdapter_GB_WriteSB;
	adapter->gbDriver.writeSC = MobileAdapter_GB_WriteSC;

	// GameBoy Advance driver init (NORMAL8)
	adapter->gbaDriver.init = MobileAdapter_AGB_Init;
	adapter->gbaDriver.deinit = MobileAdapter_AGB_Deinit;
	adapter->gbaDriver.load = /*MobileAdapter_AGB_Load*/ NULL;
	adapter->gbaDriver.unload = /*MobileAdapter_AGB_Unload*/ NULL;
	adapter->gbaDriver.writeRegister = MobileAdapter_AGB_WriteRegister;

	// Common callbacks
	adapter->dial = NULL;
	adapter->connect = NULL;
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

void MobileAdapter_ResetServerData(struct MobileAdapterGBExternalServerData* server) {
	memset(server->address, 0, 16);
	server->isConnectionOpened = false;
	server->port = 0;
}

void MobileAdapter_Init(struct MobileAdapterGB* adapter) {
	MobileAdapter_ResetServerData(&adapter->server);

	adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;

	adapter->buffer = NULL;
	adapter->bufferLength = 0;
	adapter->checksum = 0;
	adapter->command = 0;
	adapter->deviceType = MOBILE_ADAPTER_GB_DEVICE_PDC;

	adapter->isLineBusy = false;
	adapter->isLoggedIn = false;
	adapter->isSending = false;
}

static void MobileAdapter_Deinit(struct MobileAdapterGB* adapter) {
	if (adapter->buffer)
		free(adapter->buffer);

	adapter->buffer = NULL;
}

static uint8_t MobileAdapter_HandleByte(struct MobileAdapterGB* adapter, uint8_t value) {
	uint8_t returnValue = 0x00;

	switch (adapter->status) {
	case MOBILE_ADAPTER_GB_STATUS_WAITING:
		if (value == 0x99 || adapter->isSending) {
			adapter->status = MOBILE_ADAPTER_GB_STATUS_PREAMBLE;
		}

		if (adapter->isSending) {
			returnValue = 0x99;
		}

		break;
	case MOBILE_ADAPTER_GB_STATUS_PREAMBLE:
		if (value == 0x66 || adapter->isSending) {
			adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_START;
		} else {
			adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;
			mLOG(CGBADAPTER, DEBUG, "Received wrong Preamble packet! Resetting...");
		}

		if (adapter->isSending) {
			returnValue = 0x66;
		}

		break;

	case MOBILE_ADAPTER_GB_STATUS_PACKET_START:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_PACKET_01;

		if (adapter->isSending) {
			returnValue = adapter->command;
		} else {
			adapter->command = value;
		}

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
			} else {
				adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1;
			}
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

		if (adapter->processedByte >= adapter->bufferLength) {
			adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1;
		}

		break;
	case MOBILE_ADAPTER_GB_STATUS_CHECKSUM_1:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_CHECKSUM_2;

		if (adapter->isSending) {
			returnValue = adapter->checksum >> 8;
		} else {
			if ((adapter->checksum >> 8) != value) {
				mLOG(CGBADAPTER, DEBUG, "Failed checksum! Resetting...");
				returnValue = 0xF1;
				adapter->status = MOBILE_ADAPTER_GB_STATUS_WAITING;
			}
		}

		break;
	case MOBILE_ADAPTER_GB_STATUS_CHECKSUM_2:
		adapter->status = MOBILE_ADAPTER_GB_STATUS_DEVICE_ID;

		if (adapter->isSending) {
			returnValue = adapter->checksum & 0xFF;
		} else {
			if ((adapter->checksum & 0xFF) != value) {
				mLOG(CGBADAPTER, DEBUG, "Failed checksum! Resetting...");
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
			MobileAdapter_ProcessData(adapter);
		}

		adapter->isSending = !adapter->isSending;
	}

	return returnValue;
}

static void MobileAdapter_ProcessData(struct MobileAdapterGB* adapter) {
	uint8_t* inputData = adapter->buffer;
	uint8_t inputDataLen = adapter->bufferLength;
	uint8_t* outputData = NULL;
	uint8_t outputDataLen = 0;

	switch (adapter->command) {
	case 0x10: // Initialization command
		outputData = (uint8_t*) malloc(inputDataLen);
		outputDataLen = inputDataLen;
		memcpy(outputData, inputData, inputDataLen);

		mLOG(CGBADAPTER, DEBUG, "Initialized! (NINTENDO)");

		break;

	case 0x19: { // Read configuration
		uint8_t offset = inputData[0];
		
		if (adapter->readConfiguration) {
			outputDataLen = inputData[1] + 1;
			outputData = (uint8_t*) malloc(outputDataLen);
			outputData[0] = offset;
			if (!adapter->readConfiguration(adapter, inputData[0], inputData[1], &outputData[1])) {
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
			outputData[0] = 0x05; // TODO: 0x04 could also be used?
		else
			outputData[0] = 0x00;

		mLOG(CGBADAPTER, DEBUG, "Status of the line: %u", outputData[0]);

		break;

	case 0x11: // Close connection
		if (adapter->disconnect)
			adapter->disconnect(adapter);

		mLOG(CGBADAPTER, INFO, "Disconnected from server %s:%u", adapter->server.address, adapter->server.port);

		MobileAdapter_ResetServerData(&adapter->server);
		adapter->isLineBusy = false;
		break;

	case 0x12: {	
		char* phone = (char*) malloc(inputDataLen + 1);
		memcpy(phone, inputData, inputDataLen);
		phone[inputDataLen] = 0;

		mLOG(CGBADAPTER, INFO, "Calling %s...", phone);

		// Ignore PDC and CDMA DION phone
		if (strcmp(phone, "#9677") != 0 && strcmp(phone, "0077487751") != 0) {
			if (adapter->dial) {
				if (adapter->dial(adapter, phone)) {
					adapter->isLineBusy = true;
				} else {
					mLOG(CGBADAPTER, INFO, "Unable to call %s", phone);
				}

				// TODO: The error packet is missing, also in py script ...
			}
		} else {
			mLOG(CGBADAPTER, DEBUG, "Fake called DION phones currectly");
			adapter->isLineBusy = true;
		}

		break;
	}

	case 0x13: // Hung up
		if (adapter->hangUp)
			adapter->hangUp(adapter);

		mLOG(CGBADAPTER, INFO, "Phone line freed");
		adapter->isLineBusy = false;
		break;

	case 0x21: { // DION Login
		outputData = (uint8_t*) malloc(1);
		outputDataLen = 1;

		if (adapter->login)
		{
			uint8_t userLen = outputData[0], passLen = outputData[1];
			if (!adapter->login(adapter, userLen, &inputData[2], passLen, &inputData[2 + userLen])) {
				outputData[0] = 0xF1;
				mLOG(CGBADAPTER, INFO, "Failed DION login");
			} else {
				outputData[0] = 0x00;
				mLOG(CGBADAPTER, INFO, "Successfully logged in to DION");

				adapter->isLoggedIn = true;
			}
		}
		else {
			outputData[0] = 0x00;
			adapter->isLoggedIn = true;
			mLOG(CGBADAPTER, INFO, "Fake DION Login succeeded");
		}

		break;
	}

	case 0x22: // DION Logout
		if (adapter->logout)
			adapter->logout(adapter);

		mLOG(CGBADAPTER, INFO, "Logged out of DION");

		adapter->isLoggedIn = false;
		break;

	case 0x28: { // DNS Query
		if (adapter->resolveDns)
		{
			char* NTdomain = (char*)malloc(inputDataLen + 1);
			memcpy(NTdomain, inputData, inputDataLen);
			NTdomain[inputDataLen] = 0;

			char* ip = (char*)adapter->resolveDns(adapter, NTdomain);
			if (!ip) {
				outputData = (uint8_t*)malloc(1);
				outputDataLen = 1;
				outputData[0] = 0xF1; // This value is not confirmed or real.

				mLOG(CGBADAPTER, ERROR, "Unable to resolve DNS query for %s\n", NTdomain);
			}
			else {
				outputDataLen = (uint8_t)strlen(ip);
				outputData = (uint8_t*)malloc(outputDataLen);
				memcpy(outputData, ip, outputDataLen);

				mLOG(CGBADAPTER, DEBUG, "Resolved DNS query %s for %s\n", ip, NTdomain);

				free(ip);
			}

			free(NTdomain);
		}
		else {
			// Hardcoded IP
			outputData = (uint8_t*)malloc(15);
			memcpy(outputData, "200.200.200.200", 15);
			outputDataLen = 15;

			mLOG(CGBADAPTER, DEBUG, "Faked DNS Query: 200.200.200.200");
		}

		break;
	}

	case 0x23: { // Connect to an external server
		MobileAdapter_ResetServerData(&adapter->server);

		adapter->server.address[0] = inputData[0];
		adapter->server.address[1] = '.';
		adapter->server.address[2] = inputData[1];
		adapter->server.address[3] = '.';
		adapter->server.address[4] = inputData[2];
		adapter->server.address[5] = '.';
		adapter->server.address[6] = inputData[3];
		adapter->server.address[7] = 0;

		adapter->server.port = (inputData[4] << 8) + inputData[5];

		adapter->command = 0xA3;
		outputDataLen = 1;
		outputData = (uint8_t*)malloc(outputDataLen);

		if (adapter->connect) {
			if (!adapter->connect(adapter)) {
				mLOG(CGBADAPTER, INFO, "Failed to connect to %s:%u", adapter->server.address, adapter->server.port);
				outputData[0] = 0x00;
			} else {
				mLOG(CGBADAPTER, INFO, "Connect to %s:%u", adapter->server.address, adapter->server.port);
				outputData[0] = 0xFF;
				adapter->server.isConnectionOpened = true;
			}
		} else {
			mLOG(CGBADAPTER, DEBUG, "Faked connection to server");
			adapter->server.isConnectionOpened = true;
		}

		break;
	}

	case 0x24: // Close connection to server
		if (adapter->disconnect) {
			adapter->disconnect(adapter);
		}

		MobileAdapter_ResetServerData(&adapter->server);
		mLOG(CGBADAPTER, INFO, "Disconnected to server");
		break;

	case 0x15: { // Send data to server
		if (adapter->server.isConnectionOpened) {
			if (inputDataLen > 0) {
				if (adapter->sendDataToServer) {
					if (!adapter->sendDataToServer(adapter, inputDataLen, inputData)) {
						outputDataLen = 1;
						outputData = (uint8_t*)malloc(outputDataLen);
						outputData[0] = 0x00;

						mLOG(CGBADAPTER, INFO, "Unable to send data to server %d", adapter->server.port);
						break;
					}
				} else {
						MobileAdapter_FakeServer(adapter, &inputDataLen, &inputData, false);
				}

				adapter->command = 0x95;
				
				if (adapter->receiveDataFromServer) {
					unsigned char* readOut = adapter->receiveDataFromServer(adapter, &outputDataLen);
					if (!readOut)
					{
						outputDataLen = 1;
						outputData = (uint8_t*)malloc(outputDataLen);
						outputData[0] = 0x00;
					} else {
						outputDataLen++;
						outputData = (uint8_t*)malloc(outputDataLen);
						memcpy(&outputData[1], readOut, outputDataLen - 1);
						outputData[0] = 0x0;

						free(readOut);
					}
				} else {
					MobileAdapter_FakeServer(adapter, &outputDataLen, &outputData, true);
				}
			}
		} else {
			adapter->command = 0x9F;
			outputDataLen = 1;
			outputData = (uint8_t*)malloc(1);
			outputData[0] = 0;
		}
		break;
	}

	default:
		mLOG(CGBADAPTER, WARN, "Unhandled command: %u\n", adapter->command);
		break;
	}

	if (inputData)
		free(inputData);

	adapter->buffer = outputData;
	adapter->bufferLength = outputDataLen;
}

void MobileAdapter_FakeServer(struct MobileAdapterGB* adapter, uint8_t* length, uint8_t** data, bool receiving) {
	if (!receiving) {
		memset(adapter->internalServerBuffer, 0, 254);
		memcpy(adapter->internalServerBuffer, *data, *length);
	} else {
		uint8_t* addData = NULL;

		if (adapter->server.port == 110) {
			if (strcmp(&adapter->internalServerBuffer[1], "STAT") > 0 || strcmp(&adapter->internalServerBuffer[1], "LIST 1") > 0) {
				addData = (uint8_t*)malloc(8);
				*length = 8;
				memcpy(&addData[1], "+OK 0 0", 7);
			} else if (strcmp(&adapter->internalServerBuffer[1], "LIST ") > 0) {
				addData = (uint8_t*)malloc(7);
				*length = 7;
				memcpy(&addData[1], "-ERR\r\n", 6);
			} else if (strcmp(&adapter->internalServerBuffer[1], "LIST") > 0) {
				*length = 38;
				addData = (uint8_t*)malloc(38);
				memcpy(&addData[1], "+OK Mailbox scan listing follows\r\n.\r\n", 37);
			} else {
				addData = (uint8_t*)malloc(6);
				*length = 6;
				memcpy(&addData[1], "+OK\r\n", 5);
			}
		} else {
			addData = (uint8_t*)malloc(1);
			*length = 1;
		}

		addData[0] = 0x00;

		*data = addData;
	}
}

// GameBoy SIO Driver
static void MobileAdapter_GB_WriteSB(struct GBSIODriver* driver, uint8_t value) {
	driver->p->pendingSB = MobileAdapter_HandleByte((struct MobileAdapterGB*) driver, value);
}

static uint8_t MobileAdapter_GB_WriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct MobileAdapterGB* adapter = (struct MobileAdapterGB*) driver;

	if (value & 1)
		value = value & ~1; // The adapter is always the slave

	return value; 
}

static bool MobileAdapter_GB_Init(struct GBSIODriver* driver) {
	MobileAdapter_Init((struct MobileAdapterGB*)driver);
	return true;
}

static void MobileAdapter_GB_Deinit(struct GBSIODriver* driver) {
	MobileAdapter_Deinit((struct MobileAdapterGB*)driver);
}

// GameBoy Advance SIO Driver
static bool MobileAdapter_AGB_Init(struct GBASIODriver* driver) {
	MobileAdapter_Init((struct MobileAdapterGB*)driver);
	return true;
}

static void MobileAdapter_AGB_Deinit(struct GBASIODriver* driver) {
	MobileAdapter_Deinit((struct MobileAdapterGB*)driver);
}

static int bitCount = 0;

static uint16_t MobileAdapter_AGB_WriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	switch (address) {
	case REG_SIOCNT:
		if (value & 0x80)
			value = value & ~0x80; // The adapter is always the slave
		break;
	case REG_SIODATA8:
		value = MobileAdapter_HandleByte((struct MobileAdapterGB*)driver, value & 0xFF);
		break;
	default:
		break;
	}

	return value;
}
