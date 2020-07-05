/*
* libmobile implementation for mGBA GB Core
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <mgba/internal/gb/sio/mobilegb.h>

#include <mgba/internal/gb/sio.h>
#include <mgba/internal/gb/gb.h>

static void GBAdapterWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBAdapterWriteSC(struct GBSIODriver* driver, uint8_t value);
static bool GBAdapterReset(struct GBSIODriver* driver);

void GBMobileAdapterCreate(struct GBMobileAdapter* adapter) {
	adapter->byte = 0;
	adapter->next = 0xD2;

	adapter->d.init = GBAdapterReset;
	adapter->d.deinit = GBAdapterReset;
	adapter->d.writeSB = GBAdapterWriteSB;
	adapter->d.writeSC = GBAdapterWriteSC;
}

void GBAdapterWriteSB(struct GBSIODriver* driver, uint8_t value) { 
	struct GBMobileAdapter* adapter = (struct GBMobileAdapter*) driver;
	adapter->next = mobile_transfer(&adapter->mobile->mobile, adapter->byte);
	adapter->byte = value;
}

static uint8_t GBAdapterWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBMobileAdapter* adapter = (struct GBMobileAdapter*) driver;
	if ((value & 0x81) == 0x81) {
		driver->p->pendingSB = adapter->next;
	}

	return value;
}

static bool GBAdapterReset(struct GBSIODriver* driver) { 
	struct GBMobileAdapter* adapter = (struct GBMobileAdapter*) driver;
	mobile_serial_reset(&adapter->mobile->mobile);
	return true;
}
