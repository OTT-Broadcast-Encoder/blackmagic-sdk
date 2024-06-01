/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation (the
** "Software") to use, reproduce, display, distribute, sub-license, execute,
** and transmit the Software, and to prepare derivative works of the Software,
** and to permit third-parties to whom the Software is furnished to do so, in
** accordance with:
**
** (1) if the Software is obtained from Blackmagic Design, the End User License
** Agreement for the Software Development Kit (“EULA”) available at
** https://www.blackmagicdesign.com/EULA/DeckLinkSDK; or
**
** (2) if the Software is obtained from any third party, such licensing terms
** as notified by that third party,
**
** and all subject to the following:
**
** (3) the copyright notices in the Software and this entire statement,
** including the above license grant, this restriction and the following
** disclaimer, must be included in all copies of the Software, in whole or in
** part, and all derivative works of the Software, unless such copies or
** derivative works are solely in the form of machine-executable object code
** generated by a source language processor.
**
** (4) THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
**
** A copy of the Software is available free of charge at
** https://www.blackmagicdesign.com/desktopvideo_sdk under the EULA.
**
** -LICENSE-END-
*/

#pragma once

#include <chrono>
#include <time.h>
#include "DeckLinkAPI.h"

namespace ReferenceTime
{
	using ReferenceDuration = std::chrono::microseconds;		// Capture reference times in microseconds
	constexpr BMDTimeScale	kTimescale = (BMDTimeScale)ReferenceDuration(std::chrono::seconds(1)).count();
	constexpr int			kTicksPerMilliSec = ReferenceDuration(std::chrono::milliseconds(1)).count();
	constexpr long			kTicksPerNanoSec = std::chrono::nanoseconds(std::chrono::seconds(1)).count();

	static inline BMDTimeValue getSteadyClockUptimeCount(void)
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

		auto now = std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(ts.tv_sec * kTicksPerNanoSec + ts.tv_nsec));	
		auto referenceTime = std::chrono::time_point_cast<ReferenceDuration>(now);
		return referenceTime.time_since_epoch().count();
	}
};
