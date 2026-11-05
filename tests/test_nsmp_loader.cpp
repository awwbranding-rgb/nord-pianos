#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>

static inline uint16_t be16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}
static inline uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

const int PREDICTOR_COEFF[8][8] = {
    {0},
    {1},
    {2, -1},
    {3, -3, 1},
    {4, -6, 4, -1},
    {5, -10, 10, -5, 1},
    {6, -15, 20, -15, 6, -1},
    {7, -21, 35, -35, 21, -7, 1}
};

struct NsmpStrokeData {
    uint32_t gid = 0;
    int channels = 2;
    uint16_t sampleRate = 35002;
    size_t payloadOffset = 0;
    size_t payloadSize = 0;
    uint8_t rootKey = 60;
    std::vector<int16_t> pcm; // interleaved
};

bool decodeNsmpStroke(const uint8_t* bytes, size_t totalBytes,
                      size_t payloadOffset, size_t payloadSize,
                      int channelCount, std::vector<int16_t>& outInterleaved) {
    const size_t endOffset = payloadOffset + payloadSize;
    if (endOffset > totalBytes) return false;
    if (channelCount != 1 && channelCount != 2) channelCount = 2;

    for (size_t hdr = 0x60; hdr <= 0x180; hdr += 4) {
        size_t o = payloadOffset + hdr;
        if (o >= endOffset) break;

        std::vector<int32_t> chOut[2];
        int32_t ring[2][32] = {{0}, {0}};
        int head[2] = {0, 0};
        bool valid = true;
        int blockCount = 0;

        while (o + 4 <= endOffset) {
            uint32_t word = be32(bytes + o);
            o += 4;
            uint32_t sampleCnt = word & 0x3fff;
            uint32_t filterOrder = (word >> 14) & 0xf;
            uint32_t bitWidth = ((word >> 19) & 0xf) + 1;

            if (sampleCnt == 0) continue; // segment marker
            if (filterOrder > 7) { break; }
            blockCount++;

            uint32_t perCh = sampleCnt / channelCount;
            uint32_t wordsPerCh = (perCh * bitWidth + 31) / 32;
            if (o + size_t(wordsPerCh) * channelCount * 4 > endOffset) { break; }

            for (int ch = 0; ch < channelCount; ++ch) {
                uint32_t count = (ch == channelCount - 1) ? (sampleCnt - perCh * (channelCount - 1)) : perCh;
                uint64_t acc = 0;
                int nbits = 0;
                uint32_t produced = 0;

                for (uint32_t w = 0; w < wordsPerCh; ++w) {
                    uint32_t wVal = be32(bytes + o + (w * channelCount + ch) * 4);
                    acc = ((acc | (uint64_t(wVal) << (32 - nbits))));
                    nbits += 32;

                    while (nbits >= int(bitWidth) && produced < count) {
                        uint64_t raw_sample = (acc >> (64 - bitWidth)) & ((1ULL << bitWidth) - 1);
                        int32_t res = (raw_sample >= (1ULL << (bitWidth - 1))) ?
                                      int32_t(raw_sample - (1ULL << bitWidth)) : int32_t(raw_sample);

                        int64_t pred = 0;
                        for (uint32_t k = 0; k < filterOrder; ++k) {
                            pred += int64_t(PREDICTOR_COEFF[filterOrder][k]) * int64_t(ring[ch][(head[ch] - 1 - k) & 31]);
                        }
                        int32_t sample = int32_t(res + pred);
                        ring[ch][head[ch]] = sample;
                        head[ch] = (head[ch] + 1) & 31;
                        chOut[ch].push_back(sample);
                        produced++;

                        acc = (acc << bitWidth);
                        nbits -= bitWidth;
                    }
                }
            }
            o += size_t(wordsPerCh) * channelCount * 4;
        }

        if (hdr == 0x60) {
            std::cout << "Debug hdr 0x60: valid=" << valid << " o=" << o << " endOffset=" << endOffset 
                      << " diff=" << (long long)endOffset - (long long)o 
                      << " chOut[0].size()=" << chOut[0].size() << " blocks=" << blockCount << "\n";
        }

        if (valid && o >= endOffset - 8 && chOut[0].size() > 100) {
            int64_t peak = 0;
            for (auto s : chOut[0]) peak = std::max(peak, int64_t(std::abs(s)));
            if (peak < (1 << 26)) {
                // Succeeded! Scale and interleave
                float scale = 1.0f;
                if (peak > 0 && peak < 16000) {
                    scale = 24000.0f / float(peak);
                    if (scale > 8.0f) scale = 8.0f; // Limit boost
                } else if (peak > 32767) {
                    scale = 32000.0f / float(peak);
                }

                size_t numFrames = chOut[0].size();
                outInterleaved.clear();
                outInterleaved.reserve(numFrames * channelCount);

                for (size_t f = 0; f < numFrames; ++f) {
                    for (int ch = 0; ch < channelCount; ++ch) {
                        int32_t v = int32_t(std::round(float(chOut[ch][f]) * scale));
                        v = std::clamp(v, -32768, 32767);
                        outInterleaved.push_back(int16_t(v));
                    }
                }
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: test_nsmp_loader <path_to_nsmp4>\n";
        return 1;
    }
    std::string path = argv[1];
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << path << "\n";
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    std::cout << "Read " << data.size() << " bytes.\n";

    if (data.size() < 44 || std::string(reinterpret_cast<const char*>(data.data()), 4) != "CBIN") {
        std::cerr << "Not CBIN\n";
        return 1;
    }

    size_t pos = 0x2c + 16;
    struct Section {
        std::string tag;
        uint32_t ver, size, payload;
    };
    std::vector<Section> sections;
    while (pos + 12 <= data.size()) {
        std::string tag(reinterpret_cast<const char*>(data.data() + pos), 4);
        uint32_t ver = be32(data.data() + pos + 4);
        uint32_t size = be32(data.data() + pos + 8);
        size_t payload = pos + 12;
        if (payload + size > data.size()) break;
        sections.push_back({tag, ver, size, uint32_t(payload)});
        pos = payload + size;
    }

    std::cout << "Sections parsed: " << sections.size() << "\n";
    std::string instName;
    for (auto& s : sections) {
        if (s.tag.find("hdr") != std::string::npos) {
            instName = reinterpret_cast<const char*>(data.data() + s.payload + 10);
            std::cout << "Instrument Name: " << instName << "\n";
            break;
        }
    }

    int strokeCount = 0;
    int successCount = 0;
    for (auto& s : sections) {
        if (s.tag.find("stk") != std::string::npos) {
            strokeCount++;
            int ch = data[s.payload + 8];
            if (ch != 1 && ch != 2) ch = 2;
            std::vector<int16_t> pcm;
            if (decodeNsmpStroke(data.data(), data.size(), s.payload, s.size, ch, pcm)) {
                successCount++;
            }
        }
    }
    std::cout << "Decoded " << successCount << " / " << strokeCount << " strokes successfully!\n";
    return 0;
}
