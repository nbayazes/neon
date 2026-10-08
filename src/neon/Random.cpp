#include "pch.h"
#include "neon-types.h"
#include "Random.h"
#include <random>

namespace neon {

namespace {
    //constexpr int RANDOM_MAX = std::numeric_limits<int>::max();
    ////std::mt19937 gen; //seed for rd(Mersenne twister)
    //std::unique_ptr<std::minstd_rand> engine;
    //std::uniform_int_distribution randomRange(0, RANDOM_MAX);

uint _state = 0;
}

//std::mt19937& InternalMt19937() { return gen; }

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint PcgRandom() {
    uint state = _state;
    _state = _state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void InitRandom() {
    _state = std::random_device{}();
}

//void InitRandom() {
//    //gen = std::mt19937(std::random_device{}());
//    engine = std::make_unique<std::minstd_rand>(std::minstd_rand(std::random_device{}()));
//}

int RandomInt(int minimum, int maximum) {
    if (minimum >= maximum)
        std::swap(minimum, maximum);

    //const uint64 span = static_cast<uint64>(static_cast<int64>(maximum) - minimum) + 1;
    //if (span > UINT32_MAX) {
    //    return static_cast<int>(PcgRandom());
    //}

    //const uint range = static_cast<uint>(span);
    uint range = uint(maximum - minimum);
    //const uint threshold = (0u - range) % range;
    uint value = PcgRandom();
    //while (value < threshold) {
    //    value = PcgRandom();
    //}
    return minimum + static_cast<int>(value % range);
}

float RandomFloat(float minimum, float maximum) {
    const float t = (float)PcgRandom() / (float)0xffffffff;
    return minimum + (maximum - minimum) * t;
}

}
