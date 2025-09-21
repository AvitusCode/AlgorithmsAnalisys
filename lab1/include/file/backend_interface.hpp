#pragma once
#include <cstdint>
#include <span>

namespace jd::file
{
class IBackend
{
public:
    virtual ~IBackend() = default;

    virtual bool read(std::span<uint8_t> data) noexcept        = 0;
    virtual bool write(std::span<const uint8_t> data) noexcept = 0;
};
} // namespace jd::file
