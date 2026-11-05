#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>

struct PitchShifterOctave {
    static constexpr size_t BUF_SIZE = 4096;
    float buffer[BUF_SIZE] = {0};
    int writeIndex = 0;
    float phase = 0.0f;
    float windowSize = 2048.0f; // ~46ms at 44.1kHz

    void prepare(double sampleRate) {
        windowSize = float(sampleRate * 0.045); // 45ms grain
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

        // Window weights (Hann)
        float w1 = 0.5f * (1.0f - std::cos(6.2831853f * phase1));
        float w2 = 0.5f * (1.0f - std::cos(6.2831853f * phase2));

        // Read delays for +1 octave (speed 2x, delay shrinks from windowSize to 0)
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

        // Advance phase (+1 octave means speed ratio is 2.0, so rel speed = 1.0)
        phase += 1.0f / windowSize;
        if (phase >= 1.0f) phase -= 1.0f;

        writeIndex = (writeIndex + 1) % BUF_SIZE;
        return out;
    }
};

int main() {
    PitchShifterOctave ps;
    ps.prepare(44100.0);

    // Feed 440 Hz sine wave for 0.5s, check zero crossings to verify output is 880 Hz!
    int zeroCrossingsIn = 0, zeroCrossingsOut = 0;
    float prevIn = 0.0f, prevOut = 0.0f;

    for (int i = 0; i < 44100; ++i) {
        float in = std::sin(2.0f * 3.14159265f * 440.0f * (float(i) / 44100.0f));
        float out = ps.processSample(in);

        if (i > 8000) { // steady state
            if (prevIn <= 0.0f && in > 0.0f) zeroCrossingsIn++;
            if (prevOut <= 0.0f && out > 0.0f) zeroCrossingsOut++;
        }
        prevIn = in;
        prevOut = out;
    }

    std::cout << "Zero crossings In: " << zeroCrossingsIn << " (Expected ~360)\n";
    std::cout << "Zero crossings Out: " << zeroCrossingsOut << " (Expected ~720 for +1 octave)\n";
    std::cout << "Pitch ratio: " << float(zeroCrossingsOut) / float(zeroCrossingsIn) << "x (Target 2.0x)\n";
    return 0;
}
