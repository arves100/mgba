/*
 * libmobile compatibility code for Qt platform
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef QTLIBMOBILE_H
#define QTLIBMOBILE_H

#include <mgba-util/common.h>
#include <mgba/feature/libmobile.h>
#include <mgba/internal/gba/sio.h>

#include <time.h>

namespace QGBA {
    struct QMobileAdapter {
	    mobile_adapter_mgba data;
	    GBASIOMobileAdapter gba;
	    // Todo: 1 (GB)
	    clock_t millis_latch;
    };
}
 
#endif
