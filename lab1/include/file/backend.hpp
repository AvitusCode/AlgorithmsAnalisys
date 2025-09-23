#pragma once
#include "file/backend_interface.hpp"

#include <string>

namespace jd::file
{
class Backend : public IBackend
{
public:
    Backend(std::string filename);

    int64_t read(std::span<std::byte> data) noexcept override;
    int64_t write(std::span<const std::byte> data) noexcept override;

private:
    std::string filename_;
};
} // namespace jd::file
