#include "file/backend.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>

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
    std::ifstream file;

    try {
        file.open(filename_, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return -1;
        }

        const auto file_size = file.tellg();
        if (file_size == -1) {
            return -1;
        }

        if (static_cast<std::uintmax_t>(file_size) < sizeof(uint32_t)) {
            return -1;
        }

        const int64_t data_size_in_file = static_cast<int64_t>(file_size) - sizeof(uint32_t);
        if (data_size_in_file <= 0) {
            return 0;
        }

        file.seekg(0, std::ios::beg);
        if (!file.good()) {
            return -1;
        }

        const size_t bytes_to_read = std::min(static_cast<size_t>(data_size_in_file), data.size());

        if (bytes_to_read > 0) {
            if (!file.read(reinterpret_cast<char*>(data.data()), bytes_to_read)) {
                return -1;
            }
        }

        if (static_cast<size_t>(data_size_in_file) > bytes_to_read) {
            file.seekg(data_size_in_file - bytes_to_read, std::ios::cur);
            if (!file.good()) {
                return -1;
            }
        }

        uint32_t file_crc = 0;
        if (!file.read(reinterpret_cast<char*>(&file_crc), sizeof(file_crc))) {
            return -1;
        }

        if (bytes_to_read > 0) {
            const uint32_t calculated_crc = calculateCRC32(std::span<const std::byte>(data.data(), bytes_to_read));

            if (file_crc != calculated_crc) {
                return -1;
            }
        } else {
            if (file_crc != 0) {
                return -1;
            }
        }

        return data_size_in_file;

    } catch (const std::exception&) {
        return -1;
    }
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
