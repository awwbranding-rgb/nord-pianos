#include "NpnoReader.h"
#include <iostream>
#include <set>
int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: check_npno file.npno\n"; return 2; }
    try {
        auto v = parseNpno(argv[1]);
        std::set<int> roots;
        unsigned attack = 0, resonance = 0, release = 0;
        for (auto& s : v.strokes) {
            roots.insert(s.root);
            if (s.bank == 0) ++attack;
            if (s.bank == 1) ++resonance;
            if (s.bank == 2) ++release;
        }
        std::cout << "NAME: " << v.name << "\nVERSION: 0x" << std::hex << v.version << std::dec
                  << "\nCHANNELS: " << v.channels << "\nSTROKES: " << v.strokes.size()
                  << "\nROOTS: " << roots.size() << "\nATTACK: " << attack << "\nRESONANCE: " << resonance
                  << "\nRELEASE: " << release << "\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
}
