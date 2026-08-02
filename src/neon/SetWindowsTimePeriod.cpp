#include "pch.h"
#include "SetWindowsTimePeriod.h"
#include <timeapi.h>

namespace neon {

SetWindowsTimePeriod::SetWindowsTimePeriod() {
    TIMECAPS tc{};
    if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR)
        _timerPeriod = tc.wPeriodMin;

    timeBeginPeriod(_timerPeriod);
}

SetWindowsTimePeriod::~SetWindowsTimePeriod() {
    timeEndPeriod(_timerPeriod);
}

}
