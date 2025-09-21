#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>

#include "file/backend_interface.hpp"

namespace jd
{
template <typename T>
concept Serializable = requires { requires std::is_trivial_v<T> && std::is_standard_layout_v<T> && not std::is_pointer_v<T> && not std::is_reference_v<T>; };

template <Serializable T>
class GameSaveManger
{
public:
    GameSaveManger(std::shared_ptr<file::IBackend> backend)
        : backend_{std::move(backend)}
    {
        assert(backend_ != nullptr);
    }

    [[nodiscard]] bool serialize(const T& data) const
    {
        return backend_->write(toBytes(data));
    }

    [[nodiscard]] bool deserialize(T& data) const
    {
        return backend_->read(toBytes(data));
    }

private:
    static std::span<const uint8_t> toBytes(const T& data)
    {
        return {reinterpret_cast<const uint8_t*>(&data), sizeof(T)};
    }

    static std::span<uint8_t> toBytes(T& data)
    {
        return {reinterpret_cast<uint8_t*>(&data), sizeof(T)};
    }

    std::shared_ptr<file::IBackend> backend_;
};
} // namespace jd
