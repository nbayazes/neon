#pragma once
#include "neon-types.h"

namespace neon {

// Header information extracted from a RIFF/WAVE file.
struct WavHeader {
    uint16 formatTag = 0; // 1 = PCM, 3 = IEEE float
    uint16 channels = 0;
    uint32 sampleRate = 0;
    uint32 byteRate = 0;
    uint16 blockAlign = 0;
    uint16 bitsPerSample = 0;
    uint32 dataOffset = 0; // byte offset of the PCM data within the file
    uint32 dataSize = 0; // size of the PCM data in bytes

    bool IsValid() const {
        return dataSize > 0 && (formatTag == 1 || formatTag == 3) && sampleRate > 0 && (channels == 1 || channels == 2);
    }
};

Option<WavHeader> ReadWavHeader(const span<const uint8>& file);

}
