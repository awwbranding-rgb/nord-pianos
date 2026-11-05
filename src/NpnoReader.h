#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

enum class NordFormatType {
    PianoNpno,
    SampleNsmp
};

struct NpnoStroke {
    std::uint8_t root = 0;
    std::uint8_t bank = 0;
    std::uint8_t layer = 0;
    std::uint32_t frames = 0;
    std::uint16_t blocks = 0;
    std::uint32_t offset = 0; // Body relative
    std::uint32_t gid = 0;    // For NSMP matching
    std::uint32_t payloadOffset = 0;
    std::uint32_t payloadSize = 0;
};

struct NpnoInfo {
    std::string name;
    std::string category = "Acoustic Piano";
    NordFormatType formatType = NordFormatType::PianoNpno;
    std::uint16_t version = 0;
    std::uint16_t channels = 2;
    std::array<std::uint8_t, 128> keyMap{};
    std::vector<NpnoStroke> strokes;
};

// Structural validation & metadata parser for both .npno and .nsmp/.nsmp4 files
NpnoInfo parseNpno(const std::string& path);
