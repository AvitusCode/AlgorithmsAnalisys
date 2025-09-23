#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace jd::file
{
class IBackend
{
public:
    virtual ~IBackend() = default;

    virtual int64_t read(std::span<std::byte> data) noexcept        = 0;
    virtual int64_t write(std::span<const std::byte> data) noexcept = 0;
};
} // namespace jd::file
