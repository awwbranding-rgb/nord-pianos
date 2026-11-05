#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <limits>
#include <cstdint>

//==============================================================================
// All-Pass Diffuser Stage
// Smeares sharp transients and granular seams into silky continuous clouds.
//==============================================================================
class AllPassDiffuser {
public:
    void init(size_t maxDelay) {
        buffer.assign(std::max(size_t(4), maxDelay), 0.0f);
        size = buffer.size();
        index = 0;
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        index = 0;
    }

    inline float process(float in, float coeff) noexcept {
        float bufOut = buffer[index];
        float vn = in + bufOut * coeff;
        float out = -vn * coeff + bufOut;
        buffer[index] = vn;
        index = (index + 1) % size;
        return out;
    }

private:
    std::vector<float> buffer;
    size_t size = 4;
    size_t index = 0;
};

//==============================================================================
// Modulated Delay Line with Linear Hermite Interpolation
//==============================================================================
class ModulatedDelay {
public:
    void init(size_t maxDelay) {
        buffer.assign(std::max(size_t(16), maxDelay), 0.0f);
        size = buffer.size();
        writeIndex = 0;
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    inline void write(float in) noexcept {
        buffer[writeIndex] = in;
        writeIndex = (writeIndex + 1) % size;
    }

    inline float read(float delaySamples) const noexcept {
        float rPos = float(writeIndex) - delaySamples;
        while (rPos < 0.0f) rPos += float(size);
        while (rPos >= float(size)) rPos -= float(size);
        size_t i0 = static_cast<size_t>(rPos) % size;
        size_t i1 = (i0 + 1) % size;
        float frac = rPos - float(i0);
        return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
    }

private:
    std::vector<float> buffer;
    size_t size = 16;
    size_t writeIndex = 0;
};

//==============================================================================
// Smooth Overlap-Add Pitch Shifter (+1 Octave / +12 Semitones)
// Dual-head crossfaded granular line with raised-cosine Hann windowing.
//==============================================================================
class SmoothOctavePitchShifter {
public:
    void prepare(double sampleRate) {
        windowSize = float(sampleRate * 0.085); // 85 ms grain for silky smooth transitions
        if (windowSize < 512.0f) windowSize = 512.0f;
        buffer.assign(16384, 0.0f);
        writeIndex = 0;
        phase = 0.0f;
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        phase = 0.0f;
    }

    inline float process(float in) noexcept {
        if (!std::isfinite(in)) in = 0.0f;
        in = std::clamp(in, -2.5f, 2.5f);
        buffer[writeIndex] = in;

        float phase1 = phase;
        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        // Raised-cosine Hann window (w1 + w2 == 1.0f at all times)
        const float w1 = 0.5f * (1.0f - std::cos(6.2831853f * phase1));
        const float w2 = 0.5f * (1.0f - std::cos(6.2831853f * phase2));

        const float delay1 = (1.0f - phase1) * windowSize;
        const float delay2 = (1.0f - phase2) * windowSize;

        const size_t bufMask = buffer.size() - 1;
        auto readInterp = [&](float d) noexcept -> float {
            float rPos = float(writeIndex) - d;
            int rInt = static_cast<int>(std::floor(rPos));
            float frac = rPos - float(rInt);
            size_t i0 = static_cast<size_t>(rInt) & bufMask;
            size_t i1 = (i0 + 1) & bufMask;
            return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
        };

        const float out = w1 * readInterp(delay1) + w2 * readInterp(delay2);

        phase += 1.0f / windowSize;
        if (phase >= 1.0f) phase -= 1.0f;

        writeIndex = (writeIndex + 1) & bufMask;
        return out;
    }

private:
    std::vector<float> buffer;
    size_t writeIndex = 0;
    float phase = 0.0f;
    float windowSize = 3748.0f;
};

//==============================================================================
// Strymon BigSky Cloud & Valhalla Shimmer Ethereal Reverb Engine
// Features 8 allpass diffusers, 4 modulated delay lines with chorus swirl,
// an octave-shifted recirculating bloom loop, and tone shaping.
//==============================================================================
class ShimmerReverbEngine {
public:
    void prepare(double sampleRate) {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        const double s = currentSampleRate / 44100.0;

        // 4 Input All-Pass Diffusers per stereo channel
        apInL[0].init(size_t(142 * s)); apInR[0].init(size_t(161 * s));
        apInL[1].init(size_t(379 * s)); apInR[1].init(size_t(397 * s));
        apInL[2].init(size_t(517 * s)); apInR[2].init(size_t(541 * s));
        apInL[3].init(size_t(701 * s)); apInR[3].init(size_t(733 * s));

        // 4 Main Tank Delay Lines with Modulation
        tankDelayL[0].init(size_t(4500 * s)); tankDelayR[0].init(size_t(4750 * s));
        tankDelayL[1].init(size_t(6200 * s)); tankDelayR[1].init(size_t(6520 * s));

        baseDelayL[0] = float(3111 * s); baseDelayR[0] = float(3341 * s);
        baseDelayL[1] = float(4923 * s); baseDelayR[1] = float(5197 * s);

        // Shimmer Smearer Diffusers (completely diffuses any pitch-shift grain seams)
        apShimL.init(size_t(613 * s));
        apShimR.init(size_t(647 * s));

        pitchShifterL.prepare(currentSampleRate);
        pitchShifterR.prepare(currentSampleRate);

        reset();
    }

    void reset() {
        for (int i = 0; i < 4; ++i) {
            apInL[i].reset();
            apInR[i].reset();
        }
        for (int i = 0; i < 2; ++i) {
            tankDelayL[i].reset();
            tankDelayR[i].reset();
            dampL[i] = dampR[i] = 0.0f;
        }
        apShimL.reset();
        apShimR.reset();
        pitchShifterL.reset();
        pitchShifterR.reset();

        lfoPhase = 0.0f;
        tankOutL = tankOutR = 0.0f;
        shimmerFeedbackL = shimmerFeedbackR = 0.0f;
    }

    void processStereo(float* left, float* right, int numSamples,
                       float reverbAmt, float shimmerAmt, float shimmerTone, bool enabled) {
        if (!enabled || (reverbAmt <= 0.02f && shimmerAmt <= 0.02f) || numSamples <= 0) return;

        const float rNorm = std::clamp(reverbAmt / 10.0f, 0.0f, 1.0f);
        const float sNorm = std::clamp(shimmerAmt / 10.0f, 0.0f, 1.0f);
        const float tNorm = std::clamp(shimmerTone / 10.0f, 0.0f, 1.0f);

        // LFO rate for the lush Strymon Cloud stereo swirl (~0.65 Hz)
        const float lfoStep = float(6.283185307 * 0.65 / currentSampleRate);
        // Feedback gain for the reverb tank (scales decay from 2.5s up to 14.0s)
        const float tankFb = 0.45f + rNorm * 0.32f;
        // Damping coefficient for the Tone knob (shapes cloud brightness)
        const float toneDamp = 0.12f + tNorm * 0.68f;
        // Recirculating Shimmer feedback gain (strictly bounded at 0.58)
        const float shimFbGain = sNorm * 0.55f;
        // Output mix levels
        const float wetRevLevel = rNorm * 0.60f;
        const float wetShimLevel = sNorm * 0.65f;

        for (int i = 0; i < numSamples; ++i) {
            float inL = std::isfinite(left[i]) ? left[i] : 0.0f;
            float inR = std::isfinite(right[i]) ? right[i] : 0.0f;

            // Stereo LFO Chorus Modulation
            lfoPhase += lfoStep;
            if (lfoPhase >= 6.2831853f) lfoPhase -= 6.2831853f;
            const float modL = std::sin(lfoPhase) * 16.0f;
            const float modR = std::cos(lfoPhase) * 16.0f;

            // 1. Multi-Stage Input All-Pass Diffusion
            float diffL = inL;
            float diffR = inR;
            diffL = apInL[0].process(diffL, 0.70f); diffR = apInR[0].process(diffR, 0.70f);
            diffL = apInL[1].process(diffL, 0.65f); diffR = apInR[1].process(diffR, 0.65f);
            diffL = apInL[2].process(diffL, 0.60f); diffR = apInR[2].process(diffR, 0.60f);
            diffL = apInL[3].process(diffL, 0.55f); diffR = apInR[3].process(diffR, 0.55f);

            // 2. Mix Shimmer Feedback into Diffused Input
            float feedL = diffL + shimmerFeedbackL * shimFbGain;
            float feedR = diffR + shimmerFeedbackR * shimFbGain;

            // Tank cross-coupling matrix
            float tInL0 = feedL + tankOutR * tankFb;
            float tInR0 = feedR + tankOutL * tankFb;

            tankDelayL[0].write(tInL0);
            tankDelayR[0].write(tInR0);

            float dL0 = tankDelayL[0].read(baseDelayL[0] + modL);
            float dR0 = tankDelayR[0].read(baseDelayR[0] + modR);

            // Damping / Tone Filter stage 1
            dampL[0] += toneDamp * (dL0 - dampL[0]);
            dampR[0] += toneDamp * (dR0 - dampR[0]);

            tankDelayL[1].write(dampL[0]);
            tankDelayR[1].write(dampR[0]);

            float dL1 = tankDelayL[1].read(baseDelayL[1] - modR);
            float dR1 = tankDelayR[1].read(baseDelayR[1] - modL);

            // Damping / Tone Filter stage 2
            dampL[1] += toneDamp * (dL1 - dampL[1]);
            dampR[1] += toneDamp * (dR1 - dampR[1]);

            tankOutL = dampL[1];
            tankOutR = dampR[1];

            // 3. Shimmer Engine: Tap Diffused Tank Output -> Pitch Shift (+1 Octave) -> Smear -> Limit
            float pL = pitchShifterL.process(tankOutL);
            float pR = pitchShifterR.process(tankOutR);

            // All-Pass Smearer (eliminates any residual graininess)
            pL = apShimL.process(pL, 0.65f);
            pR = apShimR.process(pR, 0.65f);

            // Soft-saturation tape limiter for loop stability
            shimmerFeedbackL = std::tanh(pL);
            shimmerFeedbackR = std::tanh(pR);

            // 4. Output: Original Audio + Lush Cathedral Reverb + Ethereal Shimmer Cloud
            left[i]  += tankOutL * wetRevLevel + shimmerFeedbackL * wetShimLevel;
            right[i] += tankOutR * wetRevLevel + shimmerFeedbackR * wetShimLevel;
        }
    }

private:
    double currentSampleRate = 44100.0;
    AllPassDiffuser apInL[4], apInR[4];
    ModulatedDelay tankDelayL[2], tankDelayR[2];
    float baseDelayL[2]{}, baseDelayR[2]{};
    AllPassDiffuser apShimL, apShimR;
    SmoothOctavePitchShifter pitchShifterL, pitchShifterR;

    float lfoPhase = 0.0f;
    float dampL[2]{0.0f, 0.0f};
    float dampR[2]{0.0f, 0.0f};
    float tankOutL = 0.0f, tankOutR = 0.0f;
    float shimmerFeedbackL = 0.0f, shimmerFeedbackR = 0.0f;
};
