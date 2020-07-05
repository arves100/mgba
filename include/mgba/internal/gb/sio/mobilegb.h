/*
* libmobile implementation for mGBA GB Core
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef GB_MOBILE_H
#define GB_MOBILE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gb/interface.h>
#include <mgba/feature/mobile.h>

struct GBMobileAdapter {
	struct GBSIODriver d;
	struct mMobileAdapter* mobile;
	uint8_t byte;
	uint8_t next;
};

void GBMobileAdapterCreate(struct GBMobileAdapter* adapter);

CXX_GUARD_END

#endif
