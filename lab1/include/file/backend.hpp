#pragma once
#include "file/backend_interface.hpp"

#include <string>

namespace jd::file
{
class Backend : public IBackend
{
public:
    Backend(std::string filename);

    bool read(std::span<uint8_t> data) noexcept override;
    bool write(std::span<const uint8_t> data) noexcept override;

private:
    std::string filename_;
};
} // namespace jd::file
