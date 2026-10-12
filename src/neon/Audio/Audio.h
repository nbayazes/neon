#pragma once

#include "neon-math.h"
#include "neon-types.h"

namespace neon::audio {

constexpr int SAMPLE_RATE_11KHZ = 11025;
constexpr int SAMPLE_RATE_22KHZ = 22050;

// Raw PCM waveform data
struct Waveform {
    span<const uint8> samples; // interleaved PCM samples, as raw bytes
    uint bitsPerSample = 8; // 8-bit unsigned or 16-bit signed
    uint sampleRate = SAMPLE_RATE_22KHZ;
    uint channels = 1; // mono or stereo

    bool IsValid() const {
        return !samples.empty() && (bitsPerSample == 8 || bitsPerSample == 16) && sampleRate > 0 && (channels == 1 || channels == 2);
    }
};

// Loads a waveform with a name that can be later used by the Play() functions
void LoadWaveform(string_view name, const Waveform& waveform);

void Init();

// Releases the audio device and all sounds still owned by the system.
void Shutdown();

// Updates the listener used for 3D positional audio. Call once per frame.
void SetListener(const Vector3& position, const Vector3& forward, const Vector3& up);

void Play2D(string_view sound, float gain = 1.0f, float pitch = 1.0f);
void Play3D(string_view sound, const Vector3& position, float gain = 1.0f, float pitch = 1.0f);

bool IsSoundLoaded(string_view sound);

}
