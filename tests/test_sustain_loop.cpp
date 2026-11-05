#include "NpnoDecoder.h"
#include "PianoEngine.h"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    std::cout << "Loading NSMP4 string/synth file..." << std::endl;
    std::string path = "E:\\Disco Elisson\\NORD\\Accordion_Harmonium - Nord Piano 6\\Carina Accordion 1_PS 4.1.nsmp4";
    auto bank = npno::loadBank(path);
    if (!bank) {
        std::cout << "Failed to load bank!" << std::endl;
        return 1;
    }

    std::cout << "Bank loaded: " << bank->name << ", samples count=" << bank->samples.size() << std::endl;
    for (size_t i = 0; i < std::min(size_t(5), bank->samples.size()); ++i) {
        const auto& s = bank->samples[i];
        std::cout << "Sample " << i << ": root=" << int(s.root) << " frames=" << s.frames 
                  << " isLooped=" << s.isLooped << " loopStart=" << s.loopStart << " loopEnd=" << s.loopEnd << std::endl;
    }

    PianoEngine engine;
    engine.prepare(44100.0);
    engine.getLayerAConfig().enabled.store(true);
    engine.getLayerAConfig().volume.store(1.0f);
    engine.setLayerABank(bank);

    std::cout << "\nTriggering NoteOn (note=60, vel=100)..." << std::endl;
    engine.noteOn(60, 100);

    constexpr int blockSize = 512;
    std::vector<float> bufL(blockSize);
    std::vector<float> bufR(blockSize);

    // Render 5 seconds (approx 430 blocks)
    int blocks = int(5.0 * 44100.0 / blockSize);
    for (int b = 0; b < blocks; ++b) {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        engine.render(bufL.data(), bufR.data(), blockSize);

        double rms = 0.0;
        for (int i = 0; i < blockSize; ++i) rms += bufL[i] * bufL[i];
        rms = std::sqrt(rms / blockSize);

        if (b % 40 == 0) {
            double timeSec = (b * blockSize) / 44100.0;
            std::cout << "t=" << timeSec << "s: RMS = " << rms << " (voices active = " << engine.getActiveVoiceCount() << ")" << std::endl;
        }
    }

    std::cout << "\nTriggering High NoteOn (note=98, vel=100)..." << std::endl;
    engine.allNotesOff();
    engine.noteOn(98, 100);

    for (int b = 0; b < blocks; ++b) {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        engine.render(bufL.data(), bufR.data(), blockSize);

        double rms = 0.0;
        for (int i = 0; i < blockSize; ++i) rms += bufL[i] * bufL[i];
        rms = std::sqrt(rms / blockSize);

        if (b % 80 == 0) {
            double timeSec = (b * blockSize) / 44100.0;
            std::cout << "High note t=" << timeSec << "s: RMS = " << rms << " (voices active = " << engine.getActiveVoiceCount() << ")" << std::endl;
        }
    }

    return 0;
}
