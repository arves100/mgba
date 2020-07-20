/*
* libmobile implementation for mGBA
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef FEATURE_MOBILE_H
#define FEATURE_MOBILE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <libmobile/mobile.h>

#include <mgba-util/socket.h>

#include <mgba/core/timing.h>

struct mMobileAdapter {
	struct mobile_adapter mobile;
	Socket socket[MOBILE_MAX_CONNECTIONS];
	unsigned char config[192];
	struct mTiming* timing;
	int32_t timeLeach;

	// Config modification for server domain
	// (used for automatic patch of the email domain)

	uint16_t checksum_sum;
	uint16_t checksum_del;
	char serverdomain[0x0B];
	uint32_t serverip;
	bool checksum_update;
};

void mMobile_init(struct mMobileAdapter* adapter);
void mMobile_clear(struct mMobileAdapter* adapter);

CXX_GUARD_END

#endif
