#pragma once
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>

#include "file/backend_interface.hpp"

namespace jd
{
template <typename T>
concept Serializable = requires { requires std::is_trivial_v<T> && std::is_standard_layout_v<T> && not std::is_pointer_v<T> && not std::is_reference_v<T>; };

template <size_t BufferSize>
class GameSavesManager
{
private:
    class Writer
    {
    public:
        explicit Writer(GameSavesManager& manager)
            : manager_{manager}
        {
            if (manager_.committed_) {
                throw std::runtime_error("Serializer already committed");
            }
        }

        template <Serializable T>
        Writer& write(const T& data)
        {
            if (manager_.committed_) {
                throw std::runtime_error("Cannot write after commit");
            }

            if (manager_.pos_ + sizeof(T) > BufferSize) {
                throw std::runtime_error("Buffer overflow");
            }

            const std::byte* data_bytes = reinterpret_cast<const std::byte*>(&data);
            std::copy(data_bytes, data_bytes + sizeof(T), manager_.buffer_ + manager_.pos_);
            manager_.pos_ += sizeof(T);

            return *this;
        }

        bool commit() noexcept
        {
            if (manager_.committed_) {
                return false;
            }

            std::span<const std::byte> data_to_write(manager_.buffer_, manager_.pos_);

            auto bytes = manager_.backend_->write(data_to_write);
            if (bytes != -1) {
                manager_.committed_ = true;
                return true;
            }
            return false;
        }

        size_t currentSize() const noexcept
        {
            return manager_.pos_;
        }

        size_t capacity() const noexcept
        {
            return BufferSize - manager_.pos_;
        }

    private:
        GameSavesManager& manager_;
    };

    class Reader
    {
    public:
        explicit Reader(GameSavesManager& manager)
            : manager_(manager)
        {
            if (manager_.committed_) {
                throw std::runtime_error("Serializer already committed");
            }
        }

        Reader& load() noexcept
        {
            if (data_loaded_) {
                return *this;
            }

            std::span<std::byte> read_buffer(manager_.buffer_, BufferSize);

            auto bytes = manager_.backend_->read(read_buffer);
            if (bytes != -1) {
                data_loaded_  = true;
                manager_.pos_ = bytes;
            }

            return *this;
        }

        template <Serializable T>
        Reader& read(T& data)
        {
            if (!data_loaded_) {
                throw std::runtime_error("Data not loaded. Call load() first.");
            }

            if (manager_.pos_ < sizeof(T)) {
                throw std::runtime_error("Not enough data in buffer");
            }

            T result;
            std::byte* result_bytes = reinterpret_cast<std::byte*>(&result);
            std::copy(manager_.buffer_, manager_.buffer_ + sizeof(T), result_bytes);

            std::copy(manager_.buffer_ + sizeof(T), manager_.buffer_ + manager_.pos_, manager_.buffer_);
            manager_.pos_ -= sizeof(T);

            data = std::move(result);
            return *this;
        }

        template <Serializable T>
        bool tryRead(T& result) noexcept
        {
            if (!data_loaded_ || manager_.pos_ < sizeof(T)) {
                return false;
            }

            std::byte* result_bytes = reinterpret_cast<std::byte*>(&result);
            std::copy(manager_.buffer_, manager_.buffer_ + sizeof(T), result_bytes);

            std::copy(manager_.buffer_ + sizeof(T), manager_.buffer_ + manager_.pos_, manager_.buffer_);
            manager_.pos_ -= sizeof(T);

            return true;
        }

        size_t availableBytes() const noexcept
        {
            return data_loaded_ ? manager_.pos_ : 0;
        }

        bool isLoaded() const noexcept
        {
            return data_loaded_;
        }

    private:
        GameSavesManager& manager_;
        bool data_loaded_{false};
    };

    std::shared_ptr<file::IBackend> backend_;
    std::byte buffer_[BufferSize];
    size_t pos_{};
    bool committed_{false};

public:
    explicit GameSavesManager(std::shared_ptr<file::IBackend> backend)
        : backend_{std::move(backend)}
    {
        if (!backend_) {
            throw std::invalid_argument("Backend cannot be null");
        }
    }

    GameSavesManager(const GameSavesManager&)            = delete;
    GameSavesManager& operator=(const GameSavesManager&) = delete;
    GameSavesManager(GameSavesManager&&)                 = delete;
    GameSavesManager& operator=(GameSavesManager&&)      = delete;

    Writer writer()
    {
        return Writer(*this);
    }
    Reader reader()
    {
        return Reader(*this);
    }

    void reset() noexcept
    {
        pos_       = 0;
        committed_ = false;
    }

    size_t bufferSize() const noexcept
    {
        return BufferSize;
    }
};
} // namespace jd
