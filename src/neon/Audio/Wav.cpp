#include "pch.h"
#include "Wav.h"
#include "Streams.h"

namespace neon {

Option<WavHeader> ReadWavHeader(const span<const uint8>& file) {
    if (file.size() < 12)
        return {};

    StreamReader stream(file);
    auto code = stream.ReadUInt32();
    WavHeader header = {};

    if (code != MakeFourCC("RIFF"))
        return header;

    stream.Seek(12);

    while (!stream.EndOfStream()) {
        auto chunkHeader = stream.ReadUInt32();
        auto chunkSize = stream.ReadUInt32();
        auto chunkDataOffset = (uint32)stream.Position();

        switch(chunkHeader) {
            case MakeFourCC("fmt "):
                if (chunkSize < 16)
                    return {};

                header.formatTag = stream.ReadUInt16();
                header.channels = stream.ReadUInt16();
                header.sampleRate = stream.ReadUInt32();
                header.byteRate = stream.ReadUInt32();
                header.blockAlign = stream.ReadUInt16();
                header.bitsPerSample = stream.ReadUInt16();
                break;

            case MakeFourCC("data"):
                header.dataOffset = chunkDataOffset;
                header.dataSize = chunkSize;
                return header;
        }

        // Seek to the next chunk
        stream.Seek(chunkDataOffset + chunkSize + (chunkSize & 1));
    }

    return header;
}
}
