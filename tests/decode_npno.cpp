#include "NpnoDecoder.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <cstring>
static void little16(std::ofstream& f, uint16_t n) { char x[2] = {char(n),char(n>>8)}; f.write(x,2); }
static void little32(std::ofstream& f, uint32_t n) { little16(f,uint16_t(n)); little16(f,uint16_t(n>>16)); }
int main(int argc, char** argv) {
    if(argc < 2) { std::cerr << "Usage: decode_npno file.npno [single_stroke.wav]\n"; return 2; }
    try {
        auto bank = npno::loadBank(argv[1]);
        size_t total = 0;
        for(auto& s : bank->samples) total += s.frames;
        std::cout << "DECODE OK: " << bank->name << " | " << bank->samples.size()
                  << " strokes | " << total << " PCM frames | " << bank->channels << " channels\n";
        const auto* s = bank->select(60, 90, 0);
        if(!s) throw std::runtime_error("No C4 attack sample");
        std::cout << "C4 attack: root " << int(s->root) << " layer " << int(s->layer)
                  << " frames " << s->frames << " peak ";
        int peak = 0; for(auto n:s->pcm) peak = std::max(peak, std::abs(int(n)));
        std::cout << peak << "\n";
        if(argc > 2) {
            std::ofstream wav(argv[2],std::ios::binary);
            if(!wav) throw std::runtime_error("Cannot write output WAV");
            const uint32_t dataSize = uint32_t(s->pcm.size()*2);
            wav.write("RIFF",4); little32(wav,36+dataSize); wav.write("WAVEfmt ",8);
            little32(wav,16); little16(wav,1); little16(wav,uint16_t(bank->channels));
            little32(wav,uint32_t(npno::SampleRate)); little32(wav,uint32_t(npno::SampleRate)*2u*uint32_t(bank->channels));
            little16(wav,uint16_t(bank->channels*2)); little16(wav,16);
            wav.write("data",4); little32(wav,dataSize);
            for(auto v:s->pcm) little16(wav,uint16_t(v));
            std::cout << "WAV written: " << argv[2] << "\n";
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << "DECODE ERROR: " << e.what() << "\n"; return 1; }
}
