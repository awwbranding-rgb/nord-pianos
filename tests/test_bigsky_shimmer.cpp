#include <iostream>
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
#include <fstream>
#include <cstdint>

static void wr16(std::ofstream& f, uint16_t n) { char x[2] = {char(n), char(n>>8)}; f.write(x, 2); }
static void wr32(std::ofstream& f, uint32_t n) { wr16(f, uint16_t(n)); wr16(f, uint16_t(n>>16)); }

//==============================================================================
// All-Pass Diffuser Stage
//==============================================================================
class AllPassDiffuser {
public:
    void init(size_t maxDelay) {
        buffer.assign(maxDelay, 0.0f);
        size = maxDelay;
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
    size_t size = 1;
    size_t index = 0;
};

//==============================================================================
// Modulated Delay Line with Linear Interpolation
//==============================================================================
class ModulatedDelay {
public:
    void init(size_t maxDelay) {
        buffer.assign(maxDelay, 0.0f);
        size = maxDelay;
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
    size_t size = 1;
    size_t writeIndex = 0;
};

//==============================================================================
// Granular Pitch Shifter (+1 Octave) with Smooth Overlap-Add
//==============================================================================
class SmoothOctavePitchShifter {
public:
    void prepare(double sampleRate) {
        windowSize = float(sampleRate * 0.085); // 85 ms grain
        buffer.assign(16384, 0.0f);
        writeIndex = 0;
        phase = 0.0f;
    }

    inline float process(float in) noexcept {
        if (!std::isfinite(in)) in = 0.0f;
        in = std::clamp(in, -2.0f, 2.0f);
        buffer[writeIndex] = in;

        float phase1 = phase;
        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        // Blackman-Harris / Hann smooth raised-cosine window
        const float w1 = 0.5f * (1.0f - std::cos(6.2831853f * phase1));
        const float w2 = 0.5f * (1.0f - std::cos(6.2831853f * phase2));

        const float delay1 = (1.0f - phase1) * windowSize;
        const float delay2 = (1.0f - phase2) * windowSize;

        auto readInterp = [&](float d) noexcept -> float {
            float rPos = float(writeIndex) - d;
            while (rPos < 0.0f) rPos += float(buffer.size());
            while (rPos >= float(buffer.size())) rPos -= float(buffer.size());
            size_t i0 = static_cast<size_t>(rPos) % buffer.size();
            size_t i1 = (i0 + 1) % buffer.size();
            float frac = rPos - float(i0);
            return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
        };

        const float out = w1 * readInterp(delay1) + w2 * readInterp(delay2);

        phase += 1.0f / windowSize;
        if (phase >= 1.0f) phase -= 1.0f;

        writeIndex = (writeIndex + 1) % buffer.size();
        return out;
    }

private:
    std::vector<float> buffer;
    size_t writeIndex = 0;
    float phase = 0.0f;
    float windowSize = 3748.0f;
};

//==============================================================================
// Full Strymon BigSky Cloud Reverb Engine
//==============================================================================
class BigSkyShimmerEngine {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        const double s = sampleRate / 44100.0;

        // 4 Input All-Pass Diffusers
        apInL[0].init(size_t(142 * s)); apInR[0].init(size_t(161 * s));
        apInL[1].init(size_t(379 * s)); apInR[1].init(size_t(397 * s));
        apInL[2].init(size_t(517 * s)); apInR[2].init(size_t(541 * s));
        apInL[3].init(size_t(701 * s)); apInR[3].init(size_t(733 * s));

        // 4 Main Tank Delay Lines with Modulation
        tankDelayL[0].init(size_t(4500 * s)); tankDelayR[0].init(size_t(4750 * s));
        tankDelayL[1].init(size_t(6200 * s)); tankDelayR[1].init(size_t(6520 * s));

        baseDelayL[0] = float(3111 * s); baseDelayR[0] = float(3341 * s);
        baseDelayL[1] = float(4923 * s); baseDelayR[1] = float(5197 * s);

        // Shimmer Smearer Diffusers (smoothes pitch shift grain transitions)
        apShimL.init(size_t(613 * s));
        apShimR.init(size_t(647 * s));

        pitchL.prepare(sampleRate);
        pitchR.prepare(sampleRate);

        lfoPhase = 0.0f;
        dampL[0] = dampL[1] = 0.0f;
        dampR[0] = dampR[1] = 0.0f;
        tankOutL = tankOutR = 0.0f;
        shimmerFeedbackL = shimmerFeedbackR = 0.0f;
    }

    void process(float inL, float inR, float& outL, float& outR,
                 float revMix, float shimmerMix, float toneVal) {
        const float sNorm = std::clamp(shimmerMix / 10.0f, 0.0f, 1.0f);
        const float rNorm = std::clamp(revMix / 10.0f, 0.0f, 1.0f);
        const float tNorm = std::clamp(toneVal / 10.0f, 0.0f, 1.0f);

        // LFO Modulation for Lush Stereo Cloud Swirl
        lfoPhase += float(2.0 * 3.1415926535 * 0.7 / sr);
        if (lfoPhase >= 6.2831853f) lfoPhase -= 6.2831853f;

        const float modL = std::sin(lfoPhase) * 18.0f;
        const float modR = std::cos(lfoPhase) * 18.0f;

        // 1. Input All-Pass Diffusion (Diffuses sharp transients into a soft cloud)
        float diffL = inL;
        float diffR = inR;
        diffL = apInL[0].process(diffL, 0.70f); diffR = apInR[0].process(diffR, 0.70f);
        diffL = apInL[1].process(diffL, 0.65f); diffR = apInR[1].process(diffR, 0.65f);
        diffL = apInL[2].process(diffL, 0.60f); diffR = apInR[2].process(diffR, 0.60f);
        diffL = apInL[3].process(diffL, 0.55f); diffR = apInR[3].process(diffR, 0.55f);

        // 2. Mix Shimmer Feedback into Tank Input
        const float fbGain = 0.35f + sNorm * 0.30f;
        float feedL = diffL + shimmerFeedbackL * sNorm * 0.70f;
        float feedR = diffR + shimmerFeedbackR * sNorm * 0.70f;

        // Tank cross-coupling matrix
        float tInL0 = feedL + tankOutR * fbGain;
        float tInR0 = feedR + tankOutL * fbGain;

        tankDelayL[0].write(tInL0);
        tankDelayR[0].write(tInR0);

        float dL0 = tankDelayL[0].read(baseDelayL[0] + modL);
        float dR0 = tankDelayR[0].read(baseDelayR[0] + modR);

        // Damping / Tone Filter
        const float dampCoeff = 0.15f + tNorm * 0.65f;
        dampL[0] += dampCoeff * (dL0 - dampL[0]);
        dampR[0] += dampCoeff * (dR0 - dampR[0]);

        tankDelayL[1].write(dampL[0]);
        tankDelayR[1].write(dampR[0]);

        float dL1 = tankDelayL[1].read(baseDelayL[1] - modR);
        float dR1 = tankDelayR[1].read(baseDelayR[1] - modL);

        dampL[1] += dampCoeff * (dL1 - dampL[1]);
        dampR[1] += dampCoeff * (dR1 - dampR[1]);

        tankOutL = dampL[1];
        tankOutR = dampR[1];

        // 3. Shimmer Path: Extract Reverberated Tail -> Octave Shift -> Diffuse Smear -> Limit
        float pL = pitchL.process(tankOutL);
        float pR = pitchR.process(tankOutR);

        // Smear out any grain transitions with All-Pass diffusers
        pL = apShimL.process(pL, 0.60f);
        pR = apShimR.process(pR, 0.60f);

        // Soft analog saturation (guarantees loop stability)
        shimmerFeedbackL = std::tanh(pL);
        shimmerFeedbackR = std::tanh(pR);

        // 4. Output: Pristine Direct Reverb Tank + Ethereal Shimmer Bloom
        outL = inL + tankOutL * (rNorm * 0.85f) + shimmerFeedbackL * (sNorm * 0.75f);
        outR = inR + tankOutR * (rNorm * 0.85f) + shimmerFeedbackR * (sNorm * 0.75f);
    }

private:
    double sr = 44100.0;
    AllPassDiffuser apInL[4], apInR[4];
    ModulatedDelay tankDelayL[2], tankDelayR[2];
    float baseDelayL[2]{}, baseDelayR[2]{};
    AllPassDiffuser apShimL, apShimR;
    SmoothOctavePitchShifter pitchL, pitchR;

    float lfoPhase = 0.0f;
    float dampL[2]{0.0f, 0.0f};
    float dampR[2]{0.0f, 0.0f};
    float tankOutL = 0.0f, tankOutR = 0.0f;
    float shimmerFeedbackL = 0.0f, shimmerFeedbackR = 0.0f;
};

int main() {
    BigSkyShimmerEngine engine;
    engine.prepare(44100.0);

    constexpr int rate = 44100;
    constexpr int total = rate * 6;
    std::vector<int16_t> pcm;
    pcm.reserve(total * 2);

    for (int i = 0; i < total; ++i) {
        // Synthesize a piano-like note: 440 Hz decaying exponential pulse
        float in = 0.0f;
        if (i < 44100) {
            float t = float(i) / 44100.0f;
            in = std::sin(2.0f * 3.14159265f * 440.0f * t) * std::exp(-t * 2.5f);
        }

        float outL = 0.0f, outR = 0.0f;
        engine.process(in, in, outL, outR, 5.0f, 6.5f, 7.0f);

        pcm.push_back(int16_t(std::clamp(outL, -1.0f, 1.0f) * 32767.0f));
        pcm.push_back(int16_t(std::clamp(outR, -1.0f, 1.0f) * 32767.0f));
    }

    std::ofstream out("C:\\Projetos\\Nord_Piano_Kit\\Nord_Piano_Reader\\bigsky_test.wav", std::ios::binary);
    if (out) {
        const auto samples = uint32_t(pcm.size() * 2);
        out.write("RIFF", 4); wr32(out, 36 + samples); out.write("WAVEfmt ", 8);
        wr32(out, 16); wr16(out, 1); wr16(out, 2); wr32(out, rate); wr32(out, rate * 4);
        wr16(out, 4); wr16(out, 16); out.write("data", 4); wr32(out, samples);
        for (auto v : pcm) wr16(out, uint16_t(v));
        std::cout << "SUCCESS: bigsky_test.wav generated! Samples: " << pcm.size() / 2 << std::endl;
    }
    return 0;
}
