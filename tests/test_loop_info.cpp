#include "NpnoDecoder.h"
#include <iostream>
int main() {
    auto b = npno::loadBank("E:\\Disco Elisson\\NORD\\Grand Pianos\\Grand Imperial XL 6.1.npno");
    for (int k = 40; k <= 80; k += 10) {
        auto* s = b->select(k, 100, 0);
        if (s) {
            std::cout << "Note " << k << ": frames=" << s->frames << " isLooped=" << s->isLooped 
                      << " lStart=" << s->loopStart << " lEnd=" << s->loopEnd 
                      << " diff=" << (s->loopEnd - s->loopStart) << "\n";
        }
    }
    return 0;
}
