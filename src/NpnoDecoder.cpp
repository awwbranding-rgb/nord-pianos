#include "NpnoDecoder.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <cmath>
#include <limits>
#include <array>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <exception>

namespace {
uint16_t be16(const uint8_t* b) { return uint16_t(uint16_t(b[0]) << 8 | b[1]); }
uint32_t be32(const uint8_t* b) { return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3]; }
void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }

// ==============================================================================
// NPNO Codec (Nord Piano Library v5 / v6)
// ==============================================================================
struct BitReader {
    const uint8_t* src;
    size_t words, index = 0;
    uint64_t bits = 0;
    int held = 0;
    int read(int width) {
        while (held < width) {
            require(index < words, "Residual field extends beyond block");
            bits |= uint64_t(be16(src + 2 * index++)) << held;
            held += 16;
        }
        const uint64_t raw = bits & ((uint64_t(1) << width) - 1);
        bits >>= width;
        held -= width;
        const int32_t sign = 1 << (width - 1);
        return int32_t((raw ^ uint64_t(sign)) - uint64_t(sign));
    }
};

int64_t predictedNpno(std::array<int64_t,4>& h, int order, int residual) {
    const static int coeff[5][5] = {{1,0,0,0,0},{1,-1,0,0,0},{1,-2,1,0,0},{1,-3,3,-1,0},{1,-4,6,-4,1}};
    int64_t value = residual;
    for (int j = 1; j <= order; ++j) value -= coeff[order][j] * h[size_t(j-1)];
    for (int j = 3; j >= 1; --j) h[size_t(j)] = h[size_t(j-1)];
    h[0] = value;
    return value;
}

npno::Sample decodeStrokeNpno(const uint8_t* body, size_t bodySize,
                              const NpnoStroke& s, const uint8_t* record, int channels) {
    constexpr size_t overlap = 64;
    const size_t blockSize = 1022 * size_t(channels);
    const size_t span = size_t(s.blocks) * blockSize;
    require(size_t(s.offset) <= bodySize && span <= bodySize - s.offset, "Audio span out of bounds");
    npno::Sample out;
    out.root = s.root; out.bank = s.bank; out.layer = s.layer;
    out.trim = be16(record + 0x34); out.decay = be32(record + 0x2e);
    out.frames = s.frames;

    // Nord Piano (.npno) are 100% unlooped acoustic recordings!
    // No looping: acoustic strings ring out naturally and decay to silence like a real Nord Stage 4.
    out.isLooped = false;
    out.loopStart = 0;
    out.loopEnd = 0;
    out.decayDbPerSec = 0.0f;
    out.pcm.reserve(size_t(s.frames) * size_t(channels));
    std::array<std::array<int64_t,4>,2> history{};
    for (int ch = 0; ch < channels; ++ch)
        for (int n = 0; n < 4; ++n)
            history[size_t(ch)][size_t(n)] = int16_t(be16(record + 0x0c + (ch*4 + 3 - n)*2));
    std::vector<std::array<int32_t,2>> prevTail;
    size_t reconstructed = 0;
    for (size_t blockIndex = 0; blockIndex < s.blocks; ++blockIndex) {
        const auto* p = body + s.offset + blockIndex * blockSize;
        const uint16_t header = be16(p);
        const int width = header & 31;
        const int order = (header >> 5) & 7;
        require(width >= 1 && width <= 16 && order <= 4, "Unsupported NPno block width/order");
        const size_t frames = (8 * (blockSize - 2)) / (size_t(width) * size_t(channels));
        require(frames >= overlap + 4 && frames < 10000, "Invalid NPno block frame count");
        const size_t owned = frames - overlap;
        BitReader reader{p + 2, (blockSize - 2) / 2};
        std::vector<std::array<int32_t,2>> decoded(frames);
        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels; ++ch) {
                auto value = predictedNpno(history[size_t(ch)], order, reader.read(width));
                require(value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max(), "Predictor overflow");
                decoded[f][size_t(ch)] = int32_t(value);
            }
        }
        if (!prevTail.empty()) {
            for (size_t i = 0; i < overlap; ++i)
                for (int ch = 0; ch < channels; ++ch)
                    require(decoded[i][size_t(ch)] == prevTail[i][size_t(ch)], "NPno block-overlap mismatch");
        }
        prevTail.assign(decoded.begin() + static_cast<ptrdiff_t>(owned), decoded.end());
        for (int ch = 0; ch < channels; ++ch)
            for (int n = 0; n < 4; ++n)
                history[size_t(ch)][size_t(n)] = decoded[owned - 1 - size_t(n)][size_t(ch)];
        for (size_t i = 0; i < owned; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                int val = decoded[i][size_t(ch)];
                require(val >= -32768 && val <= 32767, "NPno predictor sample clipping detected");
                out.pcm.push_back(static_cast<int16_t>(val));
            }
        }
        reconstructed += owned;
    }
    require(reconstructed == s.frames, "Decoded frame count differs from directory");
    return out;
}

// ==============================================================================
// NW1 Codec (Nord Sample Synth Library v2 / v3 / v4)
// ==============================================================================
const int PREDICTOR_COEFF_NW1[8][8] = {
    {0},
    {1},
    {2, -1},
    {3, -3, 1},
    {4, -6, 4, -1},
    {5, -10, 10, -5, 1},
    {6, -15, 20, -15, 6, -1},
    {7, -21, 35, -35, 21, -7, 1}
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

        while (o + 4 <= endOffset) {
            uint32_t word = be32(bytes + o);
            o += 4;
            uint32_t sampleCnt = word & 0x3fff;
            uint32_t filterOrder = (word >> 14) & 0xf;
            uint32_t bitWidth = ((word >> 19) & 0xf) + 1;

            if (sampleCnt == 0) continue; // segment marker
            if (filterOrder > 7) break;   // end of stream / padding

            uint32_t perCh = sampleCnt / channelCount;
            uint32_t wordsPerCh = (perCh * bitWidth + 31) / 32;
            if (o + size_t(wordsPerCh) * channelCount * 4 > endOffset) break;

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
                            pred += int64_t(PREDICTOR_COEFF_NW1[filterOrder][k]) * int64_t(ring[ch][(head[ch] - 1 - k) & 31]);
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

        if (valid && o >= endOffset - 8 && chOut[0].size() > 100) {
            int64_t peak = 0;
            for (auto s : chOut[0]) peak = std::max(peak, int64_t(std::abs(s)));
            if (peak < (1 << 26)) {
                float scale = 1.0f;
                if (peak > 0 && peak < 16000) {
                    scale = 24000.0f / float(peak);
                    if (scale > 8.0f) scale = 8.0f;
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
}

namespace npno {
const Sample* Bank::select(int note, int velocity, int whichBank) const {
    if (note < 0 || note > 127 || whichBank < 0 || whichBank > 2) return nullptr;
    const auto root = roots[size_t(note)];
    if (root == 255) return nullptr;
    const auto& v = groups[size_t(root)][size_t(whichBank)];
    if (v.empty()) return nullptr;

    uint8_t largest = 0;
    for (const size_t idx : v) largest = std::max(largest, samples[idx].layer);
    const float threshold = float(127 - std::clamp(velocity,1,127)) * float(largest) / 126.0f;
    const Sample* selected = &samples[v.front()];
    bool found = false;
    for (const size_t idx : v) {
        const auto* candidate = &samples[idx];
        if (candidate->layer <= threshold) {
            if (!found || candidate->layer > selected->layer) selected = candidate;
            found = true;
        } else if (!found && candidate->layer < selected->layer) selected = candidate;
    }
    return selected;
}

std::shared_ptr<Bank> loadBank(const std::string& filename) {
    const auto info = parseNpno(filename);
    std::ifstream f(filename, std::ios::binary);
    require(bool(f), "Nao foi possivel abrir o arquivo Nord");
    const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>{f}, {});

    auto bank = std::make_shared<Bank>();
    bank->name = info.name;
    bank->version = info.version;
    bank->channels = int(info.channels);
    bank->roots = info.keyMap;

    if (info.formatType == NordFormatType::PianoNpno) {
        constexpr size_t bodyAt = 44;
        require(bytes.size() > bodyAt + 0x732, "Cabecalho NPno truncado");
        const uint8_t* d = bytes.data() + bodyAt;
        const size_t bodySize = bytes.size() - bodyAt;
        bank->gainTenthsDb = int(int8_t(d[0x40c]));
        const uint8_t dTop = d[0x40d];
        bank->damperTop = (dTop >= 60 && dTop <= 127) ? dTop : 92;
        for (size_t k = 0; k < 128; ++k) bank->fineTune[k] = int8_t(d[0x18c + k]);

        const size_t totalStrokes = info.strokes.size();
        bank->samples.resize(totalStrokes);

        std::exception_ptr errorPtr = nullptr;
        std::mutex errorMutex;

        const unsigned numWorkers = std::max(2u, std::min(std::thread::hardware_concurrency(), 16u));
        std::vector<std::thread> workers;
        workers.reserve(numWorkers);

        for (unsigned t = 0; t < numWorkers; ++t) {
            workers.emplace_back([t, numWorkers, totalStrokes, &info, d, bodySize, bank, &errorPtr, &errorMutex]() {
                for (size_t i = t; i < totalStrokes; i += numWorkers) {
                    if (errorPtr) break;
                    try {
                        const auto& s = info.strokes[i];
                        if (s.bank > 2) throw std::runtime_error("Tipo de banco NPno inesperado");
                        const uint8_t* record = d + 0x732 + 118 * i;
                        bank->samples[i] = decodeStrokeNpno(d, bodySize, s, record, info.channels);
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(errorMutex);
                        if (!errorPtr) errorPtr = std::current_exception();
                        break;
                    }
                }
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        if (errorPtr) std::rethrow_exception(errorPtr);

        for (size_t i = 0; i < totalStrokes; ++i) {
            const auto& s = info.strokes[i];
            if (s.bank == 0) ++bank->numAttack;
            else if (s.bank == 1) ++bank->numResonance;
            else if (s.bank == 2) ++bank->numRelease;
            bank->groups[s.root][s.bank].push_back(i);
        }
        return bank;
    }

    // ==========================================
    // Nord Sample Library (.nsmp / .nsmp4)
    // ==========================================
    bank->gainTenthsDb = 0;
    bank->damperTop = 127;
    bank->isSampleSynth = true;
    bank->numAttack = 0;
    bank->numResonance = 0;
    bank->numRelease = 0;

    const size_t totalStrokes = info.strokes.size();
    std::vector<Sample> decodedSamples(totalStrokes);
    std::vector<bool> validSample(totalStrokes, false);

    const unsigned numWorkers = std::max(2u, std::min(std::thread::hardware_concurrency(), 16u));
    std::vector<std::thread> workers;
    workers.reserve(numWorkers);

    for (unsigned t = 0; t < numWorkers; ++t) {
        workers.emplace_back([t, numWorkers, totalStrokes, &info, &bytes, bank, &decodedSamples, &validSample]() {
            for (size_t i = t; i < totalStrokes; i += numWorkers) {
                const auto& s = info.strokes[i];
                std::vector<int16_t> pcm;
                if (decodeNsmpStroke(bytes.data(), bytes.size(), s.payloadOffset, s.payloadSize, bank->channels, pcm)) {
                    Sample sample;
                    sample.root = s.root;
                    sample.bank = 0;
                    sample.layer = s.layer;
                    sample.frames = pcm.size() / size_t(bank->channels);
                    sample.pcm = std::move(pcm);
                    sample.trim = 0;
                    sample.decay = 0;
                    sample.isLooped = (sample.frames > 120);
                    if (sample.isLooped) {
                        sample.loopStart = size_t(double(sample.frames) * 0.20);
                        sample.loopEnd   = size_t(double(sample.frames) * 0.96);
                    }
                    decodedSamples[i] = std::move(sample);
                    validSample[i] = true;
                }
            }
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    for (size_t i = 0; i < totalStrokes; ++i) {
        if (validSample[i]) {
            size_t sampleIdx = bank->samples.size();
            bank->samples.push_back(std::move(decodedSamples[i]));
            bank->groups[info.strokes[i].root][0].push_back(sampleIdx);
            bank->numAttack++;
        }
    }
    require(!bank->samples.empty(), "Nao foi possivel decodificar as amostras do arquivo .nsmp4");
    return bank;
}
}
