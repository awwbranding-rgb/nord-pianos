#include "PianoEngine.h"
#include <iostream>
#include <fstream>
#include <vector>

void writeWav(const std::string& path, const std::vector<float>& left, const std::vector<float>& right, int sampleRate) {
    std::ofstream f(path, std::ios::binary);
    int numSamples = (int)left.size();
    int dataSize = numSamples * 2 * 2;
    int chunkSize = 36 + dataSize;
    f.write("RIFF", 4);
    f.write((char*)&chunkSize, 4);
    f.write("WAVEfmt ", 8);
    int subchunk1Size = 16;
    short audioFormat = 1;
    short numChannels = 2;
    int sRate = sampleRate;
    int byteRate = sampleRate * 4;
    short blockAlign = 4;
    short bitsPerSample = 16;
    f.write((char*)&subchunk1Size, 4);
    f.write((char*)&audioFormat, 2);
    f.write((char*)&numChannels, 2);
    f.write((char*)&sRate, 4);
    f.write((char*)&byteRate, 4);
    f.write((char*)&blockAlign, 2);
    f.write((char*)&bitsPerSample, 2);
    f.write("data", 4);
    f.write((char*)&dataSize, 4);
    for (int i = 0; i < numSamples; ++i) {
        short l = (short)(std::clamp(left[i], -1.0f, 1.0f) * 32767.0f);
        short r = (short)(std::clamp(right[i], -1.0f, 1.0f) * 32767.0f);
        f.write((char*)&l, 2);
        f.write((char*)&r, 2);
    }
}

int main() {
    try {
        std::cout << "Loading Layer A: Black Upright Med 6.1..." << std::endl;
        auto bankA = npno::loadBank("C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste\\Black Upright Med 6.1.npno");
        std::cout << "Layer A loaded. Samples: " << bankA->samples.size() << std::endl;

        std::cout << "Loading Layer B: Smooth Lead 3..." << std::endl;
        auto bankB = npno::loadBank("E:\\Disco Elisson\\NORD\\Synth - Nord Piano 6\\Smooth Lead 3_Mini CL 4.1.nsmp4");
        std::cout << "Layer B loaded. Samples: " << bankB->samples.size() << std::endl;

        PianoEngine engine;
        engine.prepare(44100.0);
        engine.setLayerABank(bankA, "Black Upright Med 6.1", "", "Upright Pianos");
        engine.setLayerBBank(bankB, "Smooth Lead 3", "", "Synth Lead");

        // 50/50 mix
        engine.setMixBalance(0.5f);
        engine.setEqTreble(3.0f);
        engine.setEqBass(2.0f);
        engine.setChorusDepth(4.0f);
        engine.setChorusRate(3.0f);

        std::vector<float> left(44100 * 3, 0.0f);
        std::vector<float> right(44100 * 3, 0.0f);

        std::cout << "Triggering chord C4 + E4 + G4..." << std::endl;
        engine.noteOn(60, 100);
        engine.noteOn(64, 90);
        engine.noteOn(67, 95);

        engine.render(left.data(), right.data(), 44100 * 2);

        std::cout << "Note off..." << std::endl;
        engine.noteOff(60);
        engine.noteOff(64);
        engine.noteOff(67);

        engine.render(left.data() + 44100 * 2, right.data() + 44100 * 2, 44100 * 1);

        writeWav("C:\\Projetos\\Nord_Piano_Kit\\Nord_Piano_Reader\\test_dual_layer.wav", left, right, 44100);
        std::cout << "Saved test_dual_layer.wav successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
