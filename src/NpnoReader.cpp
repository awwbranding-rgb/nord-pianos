#include "NpnoReader.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <limits>
#include <cmath>

namespace {
std::uint16_t be16(const std::uint8_t* p) {
    return (std::uint16_t(p[0]) << 8) | p[1];
}
std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16)
           | (std::uint32_t(p[2]) << 8) | p[3];
}
void expect(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
}

NpnoInfo parseNpno(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    expect(file.good(), "Cannot open file");
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>{file}, {});
    constexpr std::size_t header = 44;
    expect(bytes.size() > header, "File too short");
    const auto* f = bytes.data();
    expect(std::equal(f, f + 4, "CBIN"), "Missing CBIN header");

    const bool isNpno = std::equal(f + 8, f + 12, "npno");
    const bool isNsmp = std::equal(f + 8, f + 12, "nsmp");
    expect(isNpno || isNsmp, "Unsupported format (must be .npno or .nsmp4)");

    if (isNpno) {
        constexpr std::size_t dirAt = 0x732;
        constexpr std::size_t recordSize = 118;
        expect(bytes.size() > header + dirAt, "File too short for NPno");
        const auto* d = f + header;
        const auto bodyLen = bytes.size() - header;
        expect(std::equal(d, d + 4, "CNSP"), "Missing CNSP body");

        NpnoInfo info;
        info.formatType = NordFormatType::PianoNpno;
        info.version = be16(d + 4);
        info.channels = be16(d + 0x61e);
        expect(info.version == 0x450 || info.version == 0x464, "Unsupported CNSP version");
        expect(info.channels == 1 || info.channels == 2, "Unexpected channel count");
        expect(be16(d + 0x61c) == info.version, "Inconsistent CNSP version");

        const auto count = be16(d + 0x620);
        const std::size_t dirEnd = dirAt + std::size_t(count) * recordSize;
        expect(dirEnd < bodyLen, "Truncated stroke directory");

        std::vector<std::uint8_t> roots;
        roots.reserve(count);
        for (unsigned root = 0; root < 128; ++root) {
            const auto n = be16(d + 0x622 + root * 2);
            expect(roots.size() + n <= count, "Stroke count overflow");
            roots.insert(roots.end(), n, static_cast<std::uint8_t>(root));
        }
        expect(roots.size() == count, "Stroke count mismatch");
        std::copy(d + 0x8c, d + 0x8c + 128, info.keyMap.begin());

        const std::uint32_t stride = 1022u * info.channels;
        const std::size_t first = ((dirEnd + 192 + stride - 1) / stride) * stride - 192;
        expect(first < bodyLen, "No audio blocks");
        expect(std::all_of(d + dirEnd, d + first, [](auto b) { return b == 0; }), "Invalid alignment padding");

        std::size_t pos = first;
        for (std::size_t i = 0; i < roots.size(); ++i) {
            const auto* p = d + dirAt + i * recordSize;
            const auto start = be32(p);
            const auto blocks = be16(p + 10);
            expect(start == pos, "Audio span is discontinuous");
            expect(pos + std::size_t(blocks) * stride <= bodyLen, "Audio span exceeds file");
            info.strokes.push_back({roots[i], p[4], p[5], be32(p + 6), blocks, start, 0, 0, 0});
            pos += std::size_t(blocks) * stride;
        }
        expect(pos == bodyLen, "Unexpected trailing audio bytes");

        for (const auto root : info.keyMap)
            expect(root == 255 || std::find(roots.begin(), roots.end(), root) != roots.end(), "Key references missing root");

        const auto* name = reinterpret_cast<const char*>(d + 0x3c);
        const auto end = std::find(name, name + 32, '\0');
        info.name.assign(name, end);
        info.category = "Nord Piano Library";
        return info;
    }

    // ==========================================
    // NSMP / NSMP4 Parser (Nord Sample Synth)
    // ==========================================
    NpnoInfo info;
    info.formatType = NordFormatType::SampleNsmp;
    info.version = 4;
    info.category = "Nord Sample Synth";

    // Parse sections
    size_t pos = 0x2c + 16;
    struct Section {
        std::string tag;
        uint32_t ver, size, payload;
    };
    std::vector<Section> sections;
    while (pos + 12 <= bytes.size()) {
        std::string tag(reinterpret_cast<const char*>(bytes.data() + pos), 4);
        uint32_t ver = be32(bytes.data() + pos + 4);
        uint32_t size = be32(bytes.data() + pos + 8);
        size_t payload = pos + 12;
        if (payload + size > bytes.size()) break;
        sections.push_back({tag, ver, size, uint32_t(payload)});
        pos = payload + size;
    }

    // Instrument Name from \0hdr
    for (const auto& s : sections) {
        if (s.tag.find("hdr") != std::string::npos && s.payload + 10 < bytes.size()) {
            const auto* nameStart = reinterpret_cast<const char*>(bytes.data() + s.payload + 10);
            const auto* nameEnd = std::find(nameStart, nameStart + 32, '\0');
            info.name.assign(nameStart, nameEnd);
            break;
        }
    }
    if (info.name.empty()) info.name = "Nord Sample";

    // Channel count from strokes
    int detectedChannels = 2;
    for (const auto& s : sections) {
        if (s.tag.find("stk") != std::string::npos && s.payload + 9 <= bytes.size()) {
            int ch = bytes[s.payload + 8];
            if (ch == 1 || ch == 2) {
                detectedChannels = ch;
                break;
            }
        }
    }
    info.channels = uint16_t(detectedChannels);

    // Collect all stroke sections
    std::vector<uint32_t> stkGids;
    for (const auto& s : sections) {
        if (s.tag.find("stk") != std::string::npos && s.payload + 12 <= bytes.size()) {
            uint32_t gid = be32(bytes.data() + s.payload);
            stkGids.push_back(gid);

            NpnoStroke stroke;
            stroke.gid = gid;
            stroke.payloadOffset = s.payload;
            stroke.payloadSize = s.size;
            stroke.bank = 0; // Attack
            stroke.layer = 0;
            stroke.root = 60; // Default
            info.strokes.push_back(stroke);
        }
    }

    // Parse Zones from \0map
    info.keyMap.fill(255);
    bool zonesFound = false;

    for (const auto& s : sections) {
        if (s.tag.find("map") != std::string::npos) {
            constexpr size_t REC = 16;
            constexpr size_t PER_NOTE = 6 + 128 * 10;
            constexpr size_t HEADER = 32;
            size_t zoneStart = s.payload + PER_NOTE + HEADER;

            if (zoneStart + REC <= s.payload + s.size) {
                for (size_t o = zoneStart; o + REC <= s.payload + s.size; o += REC) {
                    uint8_t rootKey = bytes[o];
                    uint8_t keyHigh = bytes[o + 1];
                    uint8_t keyLow  = bytes[o + 2];
                    uint32_t gid = be32(bytes.data() + o + 8);
                    uint8_t lowGid = bytes[o + 11];

                    // Match GID
                    auto it = std::find(stkGids.begin(), stkGids.end(), gid);
                    if (it == stkGids.end()) {
                        it = std::find(stkGids.begin(), stkGids.end(), uint32_t(lowGid));
                    }

                    if (keyLow <= keyHigh && keyHigh <= 127 && it != stkGids.end()) {
                        size_t strokeIdx = std::distance(stkGids.begin(), it);
                        if (strokeIdx < info.strokes.size()) {
                            info.strokes[strokeIdx].root = rootKey;
                        }
                        for (int k = keyLow; k <= keyHigh; ++k) {
                            info.keyMap[size_t(k)] = rootKey;
                        }
                        zonesFound = true;
                    }
                }
            }
            break;
        }
    }

    // If no explicit zones found, map strokes evenly across keyboard
    if (!zonesFound && !info.strokes.empty()) {
        const size_t numStrokes = info.strokes.size();
        for (size_t i = 0; i < numStrokes; ++i) {
            uint8_t root = uint8_t(std::clamp(int(21 + (i * 88) / numStrokes), 0, 127));
            info.strokes[i].root = root;
        }
        for (int k = 0; k < 128; ++k) {
            size_t closestIdx = 0;
            int minDiff = 999;
            for (size_t i = 0; i < numStrokes; ++i) {
                int diff = std::abs(int(info.strokes[i].root) - k);
                if (diff < minDiff) {
                    minDiff = diff;
                    closestIdx = i;
                }
            }
            info.keyMap[size_t(k)] = info.strokes[closestIdx].root;
        }
    } else {
        // Fill unmapped notes with nearest root
        std::vector<uint8_t> validRoots;
        for (auto r : info.keyMap) {
            if (r != 255 && std::find(validRoots.begin(), validRoots.end(), r) == validRoots.end()) {
                validRoots.push_back(r);
            }
        }
        if (!validRoots.empty()) {
            for (int k = 0; k < 128; ++k) {
                if (info.keyMap[size_t(k)] == 255) {
                    uint8_t nearest = validRoots.front();
                    int minDiff = std::abs(int(nearest) - k);
                    for (auto r : validRoots) {
                        int diff = std::abs(int(r) - k);
                        if (diff < minDiff) {
                            minDiff = diff;
                            nearest = r;
                        }
                    }
                    info.keyMap[size_t(k)] = nearest;
                }
            }
        }
    }

    return info;
}
