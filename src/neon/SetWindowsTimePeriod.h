#pragma once

namespace neon {

// Set the Windows timer to be as accurate as possible
class SetWindowsTimePeriod {
    unsigned int _timerPeriod = 1; // Assume minimum resolution of 1 ms
public:
    SetWindowsTimePeriod();
    ~SetWindowsTimePeriod();

    SetWindowsTimePeriod(const SetWindowsTimePeriod&) = delete;
    SetWindowsTimePeriod(SetWindowsTimePeriod&&) = default;
    SetWindowsTimePeriod& operator=(const SetWindowsTimePeriod&) = delete;
    SetWindowsTimePeriod& operator=(SetWindowsTimePeriod&&) = default;
};

}