/*
* libmobile implementation for mGBA GBA Core
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <mgba/gba/interface.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio.h>

static bool GBASIOMobileAdapterReset(struct GBASIODriver* driver);
static uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static void GBASIOMobileAdapterTimerEvent(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBASIOMobileAdapterCreate(struct GBASIOMobileAdapter* adapter) { 
    mMobile_init(&adapter->mobile);

	adapter->d.init = NULL;
	adapter->d.deinit = NULL;
	adapter->d.load = GBASIOMobileAdapterReset;
	adapter->d.unload = GBASIOMobileAdapterReset;
	adapter->d.writeRegister = GBASIOMobileAdapterWriteRegister;

	adapter->event.context = adapter;
	adapter->event.callback = GBASIOMobileAdapterTimerEvent;
	adapter->event.priority = 0x80;

	adapter->nextbyte = 0xD2;
}

bool GBASIOMobileAdapterReset(struct GBASIODriver* driver) {
	struct GBASIOMobileAdapter* adapter = (struct GBASIOMobileAdapter*) driver;
	mobile_serial_reset(&adapter->mobile.mobile);
	return true;
}

void GBASIOMobileAdapterTimerEvent(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBASIOMobileAdapter* adapter = (struct GBASIOMobileAdapter*) user;

	adapter->d.p->siocnt = GBASIONormalClearStart(adapter->d.p->siocnt);
	if (GBASIONormalIsIrq(adapter->d.p->siocnt)) {
		GBARaiseIRQ(adapter->d.p->p, IRQ_SIO, cyclesLate);
	}
}

uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOMobileAdapter* adapter = (struct GBASIOMobileAdapter*) driver;

	switch (address) {
	case REG_SIOCNT:
		value &= ~0xC;
		value |= 0x8;
		if (value & 0x80) {
			mTimingDeschedule(&driver->p->p->timing, &adapter->event);
			mTimingSchedule(&driver->p->p->timing, &adapter->event, GBA_ARM7TDMI_FREQUENCY / 0x40000);
			break;
		}
		break;
	case REG_SIODATA8: {
		unsigned char last = mobile_transfer(&adapter->mobile.mobile, (unsigned char)value);
		value = adapter->nextbyte;
		adapter->nextbyte = last;
		break;
	}
	case REG_SIODATA32_LO:
		break;
	case REG_SIODATA32_HI:
		break;
	//case REG_RCNT:
	//	break;
	default:
		break;
	}

	return value;
}
