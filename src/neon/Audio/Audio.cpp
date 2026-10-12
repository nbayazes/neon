#include "pch.h"
#include "Audio/Audio.h"

#include "Logging.h"
#include "neon-strings.h"

// Expose the EFX function prototypes directly (OpenAL Soft exports them).
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>
#include "ankerl/ankerl.h"
#include "AudioReverb.h"

namespace neon::audio {

namespace {
    struct WaveformBuffer {
        ALuint id = 0;
    };

    CaseInsensitiveDictionary<WaveformBuffer> _buffers;

    ALCdevice* _device = nullptr;
    ALCcontext* _context = nullptr;

    bool _efx = false;
    ALuint _reverbEffect = 0;
    ALuint _reverbSlot = 0;

    constexpr int MaxSources = 64;
    constexpr float ReferenceDistance = 1.0f;
    constexpr float MaxDistance = 10000.0f;
    constexpr float RolloffFactor = 1.0f;

    List<ALuint> _sources;
    int _sourceCursor = 0;

    void CheckError(const char* where) {
        if (auto error = alGetError(); error != AL_NO_ERROR)
            SPDLOG_WARN("OpenAL error 0x{:X} in {}", (uint)error, where);
    }

    // Engine uses a left-handed coordinate system (+Z forward) while OpenAL is
    // right-handed (-Z forward). Mirror the Z axis to convert between the two.
    Vector3 ToOpenAL(const Vector3& v) {
        return { v.x, v.y, -v.z };
    }

    void ApplyReverbPreset(const EFXEAXREVERBPROPERTIES& p) {
        alEffectf(_reverbEffect, AL_EAXREVERB_DENSITY, p.flDensity);
        alEffectf(_reverbEffect, AL_EAXREVERB_DIFFUSION, p.flDiffusion);
        alEffectf(_reverbEffect, AL_EAXREVERB_GAIN, p.flGain);
        alEffectf(_reverbEffect, AL_EAXREVERB_GAINHF, p.flGainHF);
        alEffectf(_reverbEffect, AL_EAXREVERB_GAINLF, p.flGainLF);
        alEffectf(_reverbEffect, AL_EAXREVERB_DECAY_TIME, p.flDecayTime);
        alEffectf(_reverbEffect, AL_EAXREVERB_DECAY_HFRATIO, p.flDecayHFRatio);
        alEffectf(_reverbEffect, AL_EAXREVERB_DECAY_LFRATIO, p.flDecayLFRatio);
        alEffectf(_reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, p.flReflectionsGain);
        alEffectf(_reverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, p.flReflectionsDelay);
        alEffectfv(_reverbEffect, AL_EAXREVERB_REFLECTIONS_PAN, p.flReflectionsPan);
        alEffectf(_reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN, p.flLateReverbGain);
        alEffectf(_reverbEffect, AL_EAXREVERB_LATE_REVERB_DELAY, p.flLateReverbDelay);
        alEffectfv(_reverbEffect, AL_EAXREVERB_LATE_REVERB_PAN, p.flLateReverbPan);
        alEffectf(_reverbEffect, AL_EAXREVERB_ECHO_TIME, p.flEchoTime);
        alEffectf(_reverbEffect, AL_EAXREVERB_ECHO_DEPTH, p.flEchoDepth);
        alEffectf(_reverbEffect, AL_EAXREVERB_MODULATION_TIME, p.flModulationTime);
        alEffectf(_reverbEffect, AL_EAXREVERB_MODULATION_DEPTH, p.flModulationDepth);
        alEffectf(_reverbEffect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, p.flAirAbsorptionGainHF);
        alEffectf(_reverbEffect, AL_EAXREVERB_HFREFERENCE, p.flHFReference);
        alEffectf(_reverbEffect, AL_EAXREVERB_LFREFERENCE, p.flLFReference);
        alEffectf(_reverbEffect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, p.flRoomRolloffFactor);
        alEffecti(_reverbEffect, AL_EAXREVERB_DECAY_HFLIMIT, p.iDecayHFLimit);

        // Reload the effect into the slot so the changes take effect.
        alAuxiliaryEffectSloti(_reverbSlot, AL_EFFECTSLOT_EFFECT, (ALint)_reverbEffect);
    }

    // Returns a source that is free to use, growing the pool up to MaxSources.
    ALuint AcquireSource() {
        for (auto source : _sources) {
            ALint state = 0;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING)
                return source;
        }

        if ((int)_sources.size() < MaxSources) {
            ALuint source = 0;
            alGenSources(1, &source);
            alSourcef(source, AL_ROLLOFF_FACTOR, RolloffFactor);
            alSourcef(source, AL_REFERENCE_DISTANCE, ReferenceDistance);
            alSourcef(source, AL_MAX_DISTANCE, MaxDistance);
            _sources.push_back(source);
            return source;
        }

        // Every source is busy, steal one round-robin style.
        auto source = _sources[_sourceCursor];
        _sourceCursor = (_sourceCursor + 1) % (int)_sources.size();
        alSourceStop(source);
        return source;
    }

} // namespace

span<const string_view> GetReverbPresetNames() {
    static constexpr auto names = [] {
        std::array<string_view, std::size(REVERB_PRESETS)> result;
        for (uint i = 0; i < result.size(); i++)
            result[i] = REVERB_PRESETS[i].name;
        return result;
    }();

    return { names.data(), names.size() };
}

void SetReverbPreset(string_view name) {
    if (!_context || !_efx)
        return;

    for (auto& preset : REVERB_PRESETS) {
        if (String::EqualsIgnoreCase(preset.name, name)) {
            if (preset.name == REVERB_PRESETS[0].name) {
                // Detach the effect from the slot so the dry signal passes through clean.
                alAuxiliaryEffectSloti(_reverbSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
                SPDLOG_INFO("Reverb disabled");
            }
            else {
                ApplyReverbPreset(preset.props);
                SPDLOG_INFO("Reverb preset set to `{}`", name);
            }
            return;
        }
    }
}

void Init() {
    if (_context) return;

    _device = alcOpenDevice(nullptr);
    if (!_device) {
        SPDLOG_WARN("Unable to open an audio device, audio disabled");
        return;
    }

    _context = alcCreateContext(_device, nullptr);
    if (!_context) {
        SPDLOG_WARN("Unable to create an audio context, audio disabled");
        alcCloseDevice(_device);
        _device = nullptr;
        return;
    }

    alcMakeContextCurrent(_context);

    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    ALfloat orientation[6] = { 0, 0, -1, 0, 1, 0 }; // RH default: forward -Z, up +Y
    alListenerfv(AL_ORIENTATION, orientation);

    _efx = alcIsExtensionPresent(_device, "ALC_EXT_EFX") == AL_TRUE;
    if (_efx) {
        alGenEffects(1, &_reverbEffect);
        alEffecti(_reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

        alGenAuxiliaryEffectSlots(1, &_reverbSlot);
        ApplyReverbPreset(EFX_REVERB_PRESET_GENERIC);

        SPDLOG_INFO("Audio initialized with 3D audio and EFX reverb");
    }
    else {
        SPDLOG_INFO("Audio initialized with 3D audio (EFX reverb unavailable)");
    }

    CheckError("Init");
}

void Shutdown() {
    if (!_context) return;

    if (!_sources.empty()) {
        alDeleteSources((ALsizei)_sources.size(), _sources.data());
        _sources.clear();
    }

    if (_reverbSlot) {
        alDeleteAuxiliaryEffectSlots(1, &_reverbSlot);
        _reverbSlot = 0;
    }

    if (_reverbEffect) {
        alDeleteEffects(1, &_reverbEffect);
        _reverbEffect = 0;
    }

    for (auto& buffer : _buffers | views::values) {
        alDeleteBuffers(1, &buffer.id);
    }

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(_context);
    _context = nullptr;

    alcCloseDevice(_device);
    _device = nullptr;
    _efx = false;
}

void SetListener(const Vector3& position, const Vector3& forward, const Vector3& up) {
    if (!_context) return;

    auto pos = ToOpenAL(position);
    auto fwd = ToOpenAL(forward);
    auto upv = ToOpenAL(up);

    if (fwd.LengthSquared() > 0)
        fwd.Normalize();
    if (upv.LengthSquared() > 0)
        upv.Normalize();

    alListener3f(AL_POSITION, pos.x, pos.y, pos.z);

    ALfloat orientation[6] = { fwd.x, fwd.y, fwd.z, upv.x, upv.y, upv.z };
    alListenerfv(AL_ORIENTATION, orientation);
}

void LoadWaveform(string_view name, const Waveform& waveform) {
    if (!_context || !waveform.IsValid())
        return;

    if (_buffers.contains(name))
        return;

    SPDLOG_INFO("Loading waveform `{}`", name);

    ALuint id = 0;
    alGenBuffers(1, &id);
    if (id == 0) {
        CheckError("alGenBuffers");
        return;
    }

    ALenum format;
    if (waveform.channels == 2)
        format = waveform.bitsPerSample == 16 ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
    else
        format = waveform.bitsPerSample == 16 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;

    alBufferData(id, format, waveform.samples.data(), (ALsizei)waveform.samples.size(), waveform.sampleRate);
    CheckError("alBufferData");

    _buffers[name] = { .id = id };
}

void Play2D(string_view sound, float gain, float pitch) {
    if (!_context) return;

    auto buffer = FindInDictionary(_buffers, sound);
    if (!buffer) return;

    auto source = AcquireSource();

    alSourcei(source, AL_BUFFER, (ALint)buffer->id);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(source, AL_POSITION, 0, 0, 0);
    alSource3f(source, AL_VELOCITY, 0, 0, 0);
    alSourcef(source, AL_ROLLOFF_FACTOR, 0);
    alSourcef(source, AL_GAIN, gain);
    alSourcef(source, AL_PITCH, pitch);
    // 2D sounds are dry.
    alSource3i(source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
    alSourcePlay(source);
}

void Play3D(string_view sound, const Vector3& position, float gain, float pitch) {
    if (!_context) return;

    auto buffer = FindInDictionary(_buffers, sound);
    if (!buffer) return;

    auto pos = ToOpenAL(position);
    auto source = AcquireSource();

    alSourcei(source, AL_BUFFER, (ALint)buffer->id);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z);
    alSource3f(source, AL_VELOCITY, 0, 0, 0);
    alSourcef(source, AL_ROLLOFF_FACTOR, RolloffFactor);
    alSourcef(source, AL_REFERENCE_DISTANCE, ReferenceDistance);
    alSourcef(source, AL_MAX_DISTANCE, MaxDistance);
    alSourcef(source, AL_GAIN, gain);
    alSourcef(source, AL_PITCH, pitch);
    // Route 3D sounds through the reverb effect.
    alSource3i(source, AL_AUXILIARY_SEND_FILTER, _efx ? (ALint)_reverbSlot : AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
    alSourcePlay(source);
}

bool IsSoundLoaded(string_view sound) {
    return _buffers.contains(sound);
}

}
