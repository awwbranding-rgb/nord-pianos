#pragma once
#include "NpnoReader.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <array>

// All PCM is decoded on the caller's thread (never the real-time audio thread).
// The input .npno must remain the user's own file; no soundbank is embedded in the plugin.
namespace npno {
constexpr double SampleRate = 35002.0;
struct Sample {
    uint8_t root = 0, bank = 0, layer = 0;
    uint16_t trim = 0;
    uint32_t decay = 0;
    std::vector<int16_t> pcm; // interleaved channels
    size_t frames = 0;
    bool isLooped = false;
    size_t loopStart = 0;
    size_t loopEnd = 0;
    float decayDbPerSec = 1.8f; // Natural acoustic decay rate in loop (dB/s)
};
struct Bank {
    std::string name;
    uint16_t version = 0;
    int channels = 2;
    int gainTenthsDb = 0;
    uint8_t damperTop = 127;
    bool isSampleSynth = false; // true for .nsmp / .nsmp4 (Strings, Synths, Pads)
    size_t numAttack = 0;
    size_t numResonance = 0;
    size_t numRelease = 0;
    std::array<uint8_t,128> roots{};
    std::array<int8_t,128> fineTune{};
    std::vector<Sample> samples;
    // 0 attack, 1 resonance, 2 release. Indices into samples.
    std::array<std::array<std::vector<size_t>,3>,128> groups;
    const Sample* select(int midiNote, int midiVelocity, int whichBank) const;
};

// Throws std::runtime_error on corruption, unsupported versions or overlapping block mismatch.
std::shared_ptr<Bank> loadBank(const std::string& filename);
}
