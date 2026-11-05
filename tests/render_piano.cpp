#include "PianoEngine.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <stdexcept>
static void wr16(std::ofstream& f, uint16_t n) { char x[2] = {char(n),char(n>>8)}; f.write(x,2); }
static void wr32(std::ofstream& f, uint32_t n) { wr16(f,uint16_t(n)); wr16(f,uint16_t(n>>16)); }
int main(int argc,char**argv){
  if(argc<3) { std::cerr<<"Usage: render_piano sound.npno output.wav\n";return 2; }
  try {
    auto b=npno::loadBank(argv[1]); PianoEngine e; e.prepare(44100); e.setBank(b);
    constexpr int rate=44100, total=rate*6; std::vector<int16_t> pcm; pcm.reserve(total*2);
    std::vector<float> l(256),r(256); double energy=0.;
    for(int i=0;i<total;){
      // C major arpeggio and chord; MIDI notes 60, 64, 67
      if(i==0)e.noteOn(60,100);
      if(i==rate)e.noteOn(64,70);
      if(i==rate*2)e.noteOn(67,115);
      if(i==rate*3) {e.noteOff(60); e.noteOff(64); e.noteOff(67);}
      int n=std::min(256,total-i);
      for(int boundary: {rate,rate*2,rate*3}) if(i<boundary && i+n>boundary) n=boundary-i;
      std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);
      e.render(l.data(),r.data(),n);
      for(int k=0;k<n;++k){float a=std::clamp(l[k],-1.f,1.f),c=std::clamp(r[k],-1.f,1.f);energy += a*a + c*c;pcm.push_back(int16_t(a*32767.f));pcm.push_back(int16_t(c*32767.f));}
      i+=n;
    }
    const auto samples=uint32_t(pcm.size()*2);
    std::ofstream out(argv[2],std::ios::binary);if(!out)throw std::runtime_error("Cannot write output");
    out.write("RIFF",4);wr32(out,36+samples);out.write("WAVEfmt ",8);wr32(out,16);wr16(out,1);wr16(out,2);wr32(out,rate);wr32(out,rate*4);wr16(out,4);wr16(out,16);out.write("data",4);wr32(out,samples);
    for(auto v:pcm)wr16(out,uint16_t(v));
    std::cout<<"Rendered "<<total<<" frames, RMS="<<std::sqrt(energy/(total*2))<<" to "<<argv[2]<<"\n";
    return energy > 0.5 ? 0 : 1;
  } catch(const std::exception&e) {std::cerr<<"ERROR: "<<e.what()<<"\n"; return 1;}
}
