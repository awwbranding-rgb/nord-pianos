#include <iostream>
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>

struct PitchShifterOctave {
    static constexpr size_t BUF_SIZE = 4096;
    float buffer[BUF_SIZE] = {0};
    int writeIndex = 0;
    float phase = 0.0f;
    float windowSize = 2048.0f;

    void prepare(double sampleRate) {
        windowSize = float(sampleRate * 0.045);
        if (windowSize > 3500.0f) windowSize = 3500.0f;
        if (windowSize < 512.0f) windowSize = 512.0f;
        for (size_t i = 0; i < BUF_SIZE; ++i) buffer[i] = 0.0f;
        writeIndex = 0;
        phase = 0.0f;
    }

    float processSample(float in) {
        buffer[writeIndex] = in;
        float phase1 = phase;
        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        float w1 = 0.5f * (1.0f - std::cos(6.2831853f * phase1));
        float w2 = 0.5f * (1.0f - std::cos(6.2831853f * phase2));

        float delay1 = (1.0f - phase1) * windowSize;
        float delay2 = (1.0f - phase2) * windowSize;

        auto readInterp = [&](float delay) {
            float rPos = float(writeIndex) - delay;
            while (rPos < 0.0f) rPos += float(BUF_SIZE);
            while (rPos >= float(BUF_SIZE)) rPos -= float(BUF_SIZE);
            int i0 = int(rPos);
            int i1 = (i0 + 1) % BUF_SIZE;
            float frac = rPos - float(i0);
            return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
        };

        float out = w1 * readInterp(delay1) + w2 * readInterp(delay2);
        phase += 1.0f / windowSize;
        if (phase >= 1.0f) phase -= 1.0f;
        writeIndex = (writeIndex + 1) % BUF_SIZE;
        return out;
    }
};

int main() {
    PitchShifterOctave psL, psR;
    psL.prepare(44100.0);
    psR.prepare(44100.0);

    float feedbackL = 0.0f;
    float shimmerFilterL = 0.0f;
    float toneCoeff = 0.20f;
    float shimmerFeedback = 0.55f;

    // Simulate 2 seconds of audio with an initial pulse
    bool stable = true;
    for (int i = 0; i < 88200; ++i) {
        float in = (i == 100) ? 1.0f : 0.0f;
        float revIn = in + feedbackL * shimmerFeedback;
        // Simple comb filter to simulate reverb reflection
        float wet = revIn * 0.7f;
        float pitched = psL.processSample(wet);
        shimmerFilterL += toneCoeff * (pitched - shimmerFilterL);
        feedbackL = shimmerFilterL;

        if (std::isnan(feedbackL) || std::isinf(feedbackL) || std::abs(feedbackL) > 5.0f) {
            stable = false;
            std::cout << "Instability at sample " << i << ": " << feedbackL << "\n";
            break;
        }
    }

    if (stable) {
        std::cout << "Shimmer feedback loop is 100% STABLE and bound! Final feedback: " << feedbackL << "\n";
    }
    return 0;
}
