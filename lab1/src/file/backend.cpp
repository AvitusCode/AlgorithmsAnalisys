#include "file/backend.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace jd::file
{
static uint32_t calculateCRC32(std::span<const std::byte> data) noexcept
{
    uint32_t crc = 0xFFFFFFFF;
    for (auto byte : data) {
        const uint8_t val = static_cast<uint8_t>(byte);
        crc ^= val;
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

int64_t Backend::read(std::span<std::byte> data) noexcept
{
    int64_t size{-1};
    try {
        if (!fs::exists(filename_)) {
            // file does not exists
            return -1;
        }

        std::uintmax_t fileSize = fs::file_size(fs::path{filename_});
        size                    = std::min(fileSize, data.size());
        if (size < static_cast<int64_t>(sizeof(uint32_t))) {
            return -1;
        }
        size -= sizeof(uint32_t);

        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            // No such file (empty saves)
            return -1;
        }

        file.read(reinterpret_cast<char*>(data.data()), size);

        uint32_t file_crc{};
        file.read(reinterpret_cast<char*>(&file_crc), sizeof(uint32_t));

        uint32_t crc = calculateCRC32({data.data(), static_cast<size_t>(size)});

        if (file_crc != crc) {
            // std::cerr << "Bad src!" << std::endl;
            return -1;
        }

        if (!file.good()) {
            return -1;
        }
    } catch (const std::exception& ex) {
        // std::cerr << "Error with file read: " << ex.what() << std::endl;
        return -1;
    }

    return size;
}

int64_t Backend::write(std::span<const std::byte> data) noexcept
{
    try {
        std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            // std::cerr << "ERROR: cannot open a file: " << filename_ << std::endl;
            return -1;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());

        uint32_t crc = calculateCRC32(data);
        file.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

        return file.good();
    } catch (const std::exception& ex) {
        // std::cerr << "Error with file write: " << ex.what() << std::endl;
        return -1;
    }

    // return without src size
    return data.size();
}
} // namespace jd::file
