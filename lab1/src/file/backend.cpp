#include "file/backend.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace jd::file
{
static uint32_t calculateCRC32(std::span<const uint8_t> data) noexcept
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

Backend::Backend(std::string filename)
    : filename_{std::move(filename)}
{
}

bool Backend::read(std::span<uint8_t> data) noexcept
{
    try {
        if (!fs::exists(filename_)) {
            // file does not exists
            return false;
        }

        if (std::uintmax_t fileSize = fs::file_size(fs::path{filename_}); fileSize != data.size() + sizeof(uint32_t)) {
            // incorret filesize
            return false;
        }

        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            // No such file (empty saves)
            return false;
        }

        file.read(reinterpret_cast<char*>(data.data()), data.size());

        uint32_t crc{};
        file.read(reinterpret_cast<char*>(&crc), sizeof(crc));

        if (!file.good()) {
            return false;
        }
    } catch (const std::exception& ex) {
        // std::cerr << "Error with file read: " << ex.what() << std::endl;
        return false;
    }

    return true;
}

bool Backend::write(std::span<const uint8_t> data) noexcept
{
    try {
        std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            // std::cerr << "ERROR: cannot open a file: " << filename_ << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());

        uint32_t crc = calculateCRC32(data);
        file.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

        return file.good();
    } catch (const std::exception& ex) {
        // std::cerr << "Error with file write: " << ex.what() << std::endl;
        return false;
    }

    return true;
}
} // namespace jd::file
