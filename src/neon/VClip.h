#pragma once

namespace neon {

struct VClip {
    float frameTime; // time (in seconds) of each frame
    int numFrames; // Valid frames in frames
    int frames[30];
};

}