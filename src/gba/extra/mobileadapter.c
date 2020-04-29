/*
 * libmobile compatibility code for GBA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef BUILD_LIBMOBILE

#include <mgba/gba/interface.h>
#include <mgba/core/core.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio.h>

mLOG_DECLARE_CATEGORY(GBA_MOBILEADAPTER);
mLOG_DEFINE_CATEGORY(GBA_MOBILEADAPTER, "GBA MobileAdapter GB", "gba.mobileadapter");

static uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static void GBASIOMobileAdapterTimerEvent(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBASIOMobileAdapterCreate(struct GBASIOMobileAdapter* adapter, struct mobile_adapter_mgba* data) {
	if (data->platform != PLATFORM_GBA)
		return;

	adapter->d.init = NULL;
	adapter->d.deinit = NULL;
	adapter->d.load = NULL;
	adapter->d.unload = NULL;
	adapter->d.writeRegister = GBASIOMobileAdapterWriteRegister;

	adapter->event.callback = GBASIOMobileAdapterTimerEvent;
	adapter->event.context = adapter;
	adapter->event.priority = 0x80;

	mobile_init(&adapter->mobile, data, NULL); // Note: we initialize it here so we can get access to mobile_adapter_mgba structure
}

void GBASIOMobileAdapterTimerEvent(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBASIOMobileAdapter* adapter = (struct GBASIOMobileAdapter*) user;

	mobile_loop(&adapter->mobile);

	adapter->d.p->p->memory.io[REG_SIOCNT] &= 8;

	if (adapter->mobile.serial.state = 
	//adapter->d.p->siocnt = GBASIONormalClearStart(adapter->d.p->siocnt);
	if (GBASIONormalIsIrq(adapter->d.p->siocnt)) {
		GBARaiseIRQ(adapter->d.p->p, IRQ_SIO, cyclesLate);
	}
}

uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOMobileAdapter* adapter = (struct GBASIODriver*) driver;

	switch (address) {
	case REG_SIOCNT:
		if (value & 0x80) {
			int32_t cycles = GBA_ARM7TDMI_FREQUENCY / 524288;
			mTimingDeschedule(&driver->p->p->timing, &adapter->event);
			mTimingSchedule(&driver->p->p->timing, &adapter->event, cycles);
			break;
		}
		break;
	case REG_SIODATA8: {
		// transfer
		value = mobile_transfer(&adapter->mobile, (uint8_t) value);
		break;
	}
	case REG_RCNT:
		break;
	}

	return value;
}

#endif // BUILD_LIBMOBILE
