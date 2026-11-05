#include "PianoEngine.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>

static void wr16(std::ofstream& f, uint16_t n) { char x[2] = {char(n),char(n>>8)}; f.write(x,2); }
static void wr32(std::ofstream& f, uint32_t n) { wr16(f,uint16_t(n)); wr16(f,uint16_t(n>>16)); }

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: test_long_sustain sound.npno [out.wav]\n";
        return 1;
    }
    try {
        auto bank = npno::loadBank(argv[1]);
        PianoEngine engine;
        engine.prepare(44100.0);
        engine.setBank(bank);

        // Play C major chord with sustain pedal active
        engine.sustain(true);
        engine.noteOn(60, 95);
        engine.noteOn(64, 90);
        engine.noteOn(67, 92);

        constexpr int sampleRate = 44100;
        constexpr int totalSeconds = 12;
        constexpr int totalFrames = sampleRate * totalSeconds;

        std::vector<float> bufL(512), bufR(512);
        std::vector<int16_t> pcm;
        pcm.reserve(size_t(totalFrames) * 2);

        int maxDiff = 0;
        int prevL = 0;

        for (int frame = 0; frame < totalFrames; frame += 512) {
            int block = std::min(512, totalFrames - frame);
            std::fill(bufL.begin(), bufL.end(), 0.0f);
            std::fill(bufR.begin(), bufR.end(), 0.0f);
            engine.render(bufL.data(), bufR.data(), block);

            for (int k = 0; k < block; ++k) {
                int sL = int(std::clamp(bufL[k], -1.0f, 1.0f) * 32767.0f);
                int sR = int(std::clamp(bufR[k], -1.0f, 1.0f) * 32767.0f);
                if (frame > 0 || k > 0) {
                    maxDiff = std::max(maxDiff, std::abs(sL - prevL));
                }
                prevL = sL;
                pcm.push_back(int16_t(sL));
                pcm.push_back(int16_t(sR));
            }
        }

        std::cout << "Rendered " << totalSeconds << "s long sustain. Max diff: " << maxDiff << "\n";

        // Calculate RMS at 1s, 4s, 8s, 11s to verify smooth natural decay
        auto calcRms = [&](int startSec, int durSec) {
            double sum = 0.0;
            int start = startSec * sampleRate * 2;
            int count = durSec * sampleRate * 2;
            for (int i = start; i < start + count; ++i) {
                double val = double(pcm[size_t(i)]) / 32768.0;
                sum += val * val;
            }
            return std::sqrt(sum / double(count));
        };

        std::cout << "Chord sustain decay curve:\n";
        for (double sec = 0.5; sec <= 6.0; sec += 0.5) {
            int startSample = int(sec * sampleRate * 2);
            int countSamples = int(0.2 * sampleRate * 2);
            double sum = 0.0;
            for (int i = startSample; i < startSample + countSamples && i < int(pcm.size()); ++i) {
                double val = double(pcm[size_t(i)]) / 32768.0;
                sum += val * val;
            }
            double r = std::sqrt(sum / double(countSamples));
            std::cout << "  At " << sec << "s: RMS = " << r << "\n";
        }

        if (argc > 2) {
            std::ofstream out(argv[2], std::ios::binary);
            const auto samples = uint32_t(pcm.size() * 2);
            out.write("RIFF", 4); wr32(out, 36 + samples); out.write("WAVEfmt ", 8);
            wr32(out, 16); wr16(out, 1); wr16(out, 2); wr32(out, sampleRate);
            wr32(out, sampleRate * 4); wr16(out, 4); wr16(out, 16);
            out.write("data", 4); wr32(out, samples);
            for (auto v : pcm) wr16(out, uint16_t(v));
            std::cout << "WAV written to " << argv[2] << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
