#include "pch.h"
#include <chrono>
#include <thread>
#include "SystemClock.h"
#include "neon.h"


namespace neon {

SystemClock Clock;

void SystemClock::Freeze(bool frozen) {
    if (frozen) {
        ASSERT(_freezeTime == 0);
        _freezeTime = GetClockTimeNs();
    }
    else {
        ASSERT(_freezeTime != 0);
        if (_firstFrameStartTime != 0) _firstFrameStartTime += GetClockTimeNs() - _freezeTime;
        _freezeTime = 0;
        UpdateFrameTime();
    }
}

bool SystemClock::MaybeSleep(uint64 sleepMilliseconds) {
    auto milliseconds = GetTotalMilliseconds();

    if (milliseconds < _nextUpdate) {
        auto sleepTime = _nextUpdate - milliseconds;
        if (sleepTime > 1)
            std::this_thread::sleep_for(std::chrono::milliseconds((int)sleepTime - 1));

        return true;
    }
    else {
        _nextUpdate = milliseconds + sleepMilliseconds;
    }
    return false;
}

void SystemClock::Update(bool useTickRate) {
    if (useTickRate && !_freezeTime) {
        int tick = WaitForTick();
        Ticks = tick - _prevTick;
        _prevTick = tick;
    }
    else {
        UpdateFrameTime();
    }

    _frameTime = _currentFrameStartTime - _prevFrameStartTime;
    _prevFrameStartTime = _currentFrameStartTime;
}

void SystemClock::Update() {
    UpdateFrameTime();
    _frameTime = _currentFrameStartTime - _prevFrameStartTime;
    _prevFrameStartTime = _currentFrameStartTime;
}

void SystemClock::UpdateFrameTime() {
    if (_freezeTime != 0) return;
    _currentFrameStartTime = GetClockTimeNs();
    if (_firstFrameStartTime == 0) {
        _firstFrameStartTime = _prevFrameStartTime = _currentFrameStartTime;
    }
}

int SystemClock::WaitForTick() {
    int time{};
    while ((time = GetElapsedTicks()) <= _prevTick) {
        // The minimum amount of time a thread can sleep is controlled by timeBeginPeriod().
        const auto next = _firstFrameStartTime + TickToNs(_prevTick + 1, _tickRate);
        const auto now = GetClockTimeNs();
        ASSERT(next > 0);

        if (next > now) {
            const auto sleepTime = NsToMs(next - now);
            ASSERT(sleepTime < 1000);

            if (sleepTime > 2)
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime - 2));
        }

        UpdateFrameTime();
    }

    return time;
}

uint64_t SystemClock::GetClockTimeNs() const {
    using namespace std::chrono;
    auto time = (uint64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    return TimeScale == 1.0 ? time : time * (uint64_t)(TimeScale * 1000);
}

}
