#pragma once
#include "neon-types.h"

namespace neon {
class StreamReader;
}

namespace neon::d3 {
// Descent 3 Outrage Graphics File (OGF)
struct Bitmap {
    int width = 0, height = 0;
    int type = 0;
    List<List<uint>> mips;
    int bitsPerPixel = 0;
    string name;

    static Bitmap Read(StreamReader& r); // Read OGF
};

// Descent 3 Outrage Animation File (OAF). VClips are OGFs with an extra header.
struct VClip {
    List<Bitmap> frames;
    float frameTime{};
    int version{};
    bool pingPong{}; // animation reverses instead of looping
    string fileName; // the name of the file this vclip was loaded from

    uint GetFrame(double time) const {
        // todo: pingpong
        return (uint)floor(abs(time) / frameTime) % (uint)frames.size();
    }

    static VClip Read(StreamReader& r); // Read OAF
};

}
