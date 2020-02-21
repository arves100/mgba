/*
 * libmobile compatibility code for mGBA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef LIBMOBILE_H
#define LIBMOBILE_H

#ifdef BUILD_LIBMOBILE

#include <libmobile/mobile.h>

CXX_GUARD_START

struct mobile_adapter_mgba {
	uint8_t platform;
	void* extradata;
};

CXX_GUARD_END

#endif

#endif
