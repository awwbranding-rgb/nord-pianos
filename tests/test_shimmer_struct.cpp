#include <iostream>
#include <vector>
#include <cmath>

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

    // Test with pulse + decay
    std::cout << "Shimmer pitch shifter initialized successfully!\n";
    return 0;
}
