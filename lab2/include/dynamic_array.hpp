#include <cassert>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

namespace jd
{
template <typename T>
class Array final
{
    inline static constexpr int DEFAULT_CAPACITY = 16;
    inline static constexpr int ALLOCATE_FACTOR  = 2;

    T* buffer_{nullptr};
    int size_{};
    int capacity_{};

public:
    Array()
        : Array(DEFAULT_CAPACITY)
    {
    }

    explicit Array(int capacity)
        : capacity_{capacity > 0 ? capacity : DEFAULT_CAPACITY}
    {
        buffer_ = static_cast<T*>(malloc(capacity_ * sizeof(T)));
        if (!buffer_) {
            throw std::bad_alloc{};
        }
    }

    ~Array() noexcept(std::is_nothrow_destructible_v<T>)
    {
        std::destroy(buffer_, buffer_ + size_);
        free(buffer_);
    }

    Array(const Array& other)
        : size_{other.size_}
        , capacity_{other.capacity_}
    {
        buffer_ = static_cast<T*>(std::malloc(capacity_ * sizeof(T)));
        if (!buffer_) {
            throw std::bad_alloc{};
        }

        int size{};
        try {
            for (int i = 0; i < size_; ++i) {
                std::construct_at(buffer_ + i, other.buffer_[i]);
                ++size;
            }
        } catch (...) {
            if (size) {
                std::destroy(buffer_, buffer_ + size);
            }
            free(buffer_);
            throw;
        }
    }

    Array& operator=(const Array& other)
    {
        // copy swap pattern
        if (this != std::addressof(other)) {
            Array temp{other};
            std::swap(buffer_, temp.buffer_);
            std::swap(size_, temp.size_);
            std::swap(capacity_, temp.capacity_);
        }
        return *this;
    }

    Array(Array&& other) noexcept
        : buffer_{std::exchange(other.buffer_, nullptr)}
        , size_{std::exchange(other.size_, 0)}
        , capacity_{std::exchange(other.capacity_, 0)}
    {
    }

    Array& operator=(Array&& other) noexcept(std::is_nothrow_destructible_v<T>)
    {
        if (this != std::addressof(other)) {
            std::destroy(buffer_, buffer_ + size_);
            free(buffer_);

            buffer_   = std::exchange(other.buffer_, nullptr);
            size_     = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    int insert(const T& value)
    {
        return insert(size_, value);
    }

    int insert(int index, const T& value)
    {
        assert(index >= 0 && index <= size_);

        if (size_ == capacity_) {
            reallocate(capacity_ * ALLOCATE_FACTOR);
        }

        if constexpr (std::is_move_constructible_v<T> && std::is_move_assignable_v<T>) {
            if (index < size_) {
                std::construct_at(buffer_ + size_, std::move(buffer_[size_ - 1]));

                for (int i = size_ - 1; i > index; --i) {
                    buffer_[i] = std::move(buffer_[i - 1]);
                }

                buffer_[index] = value;
            } else {
                std::construct_at(buffer_ + index, value);
            }
        } else {
            if (index < size_) {
                std::construct_at(buffer_ + size_, buffer_[size_ - 1]);

                for (int i = size_ - 1; i > index; --i) {
                    buffer_[i] = buffer_[i - 1];
                }

                buffer_[index] = value;
            } else {
                std::construct_at(buffer_ + index, value);
            }
        }

        ++size_;
        return index;
    }

    void remove(int index) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>)
    {
        assert(index >= 0 && index < size_);

        std::destroy_at(buffer_ + index);

        if constexpr (std::is_move_assignable_v<T>) {
            for (int i = index; i < size_ - 1; ++i) {
                buffer_[i] = std::move(buffer_[i + 1]);
            }
        } else {
            for (int i = index; i < size_ - 1; ++i) {
                buffer_[i] = buffer_[i + 1];
            }
        }

        --size_;
        if (size_ > 0) {
            std::destroy_at(buffer_ + size_);
        }
    }

    const T& operator[](int index) const
    {
        assert(index >= 0 && index < size_);
        return buffer_[index];
    }

    T& operator[](int index)
    {
        assert(index >= 0 && index < size_);
        return buffer_[index];
    }

    int size() const noexcept
    {
        return size_;
    }

    int capacity() const noexcept
    {
        return capacity_;
    }

private:
    void reallocate(int new_capacity)
    {
        if (new_capacity <= capacity_)
            return;

        T* new_buffer = static_cast<T*>(malloc(new_capacity * sizeof(T)));
        if (!new_buffer) {
            throw std::bad_alloc{};
        }

        int size{};
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                for (int i = 0; i < size_; ++i) {
                    std::construct_at(new_buffer + i, std::move(buffer_[i]));
                    ++size;
                }
            } else {
                for (int i = 0; i < size_; ++i) {
                    std::construct_at(new_buffer + i, buffer_[i]);
                    ++size;
                }
            }
        } catch (...) {
            std::destroy(new_buffer, new_buffer + size);
            free(new_buffer);
            throw;
        }

        std::destroy(buffer_, buffer_ + size_);
        free(buffer_);

        buffer_   = new_buffer;
        capacity_ = new_capacity;
    }

private:
    template <bool Const, bool Reverse>
    class Iterator
    {
        friend class Iterator<!Const, Reverse>;
        friend class Iterator<Const, !Reverse>;

        using data_pointer = std::conditional_t<Const, const T*, T*>;

        const data_pointer start_ptr_;
        data_pointer ptr_;
        int size_;

    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = data_pointer;
        using reference         = std::conditional_t<Const, const T&, T&>;
        using iterator_category = std::forward_iterator_tag;

        Iterator(data_pointer start_ptr, data_pointer ptr, int size)
            : start_ptr_{start_ptr}
            , ptr_{ptr}
            , size_{size}
        {
        }

        reference get() const
        {
            return *ptr_;
        }

        void set(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires(!Const)
        {
            *ptr_ = value;
        }

        void next() noexcept
        {
            if constexpr (Reverse) {
                --ptr_;
            } else {
                ++ptr_;
            }
        }

        bool hasNext() const noexcept
        {
            if constexpr (Reverse) {
                return ptr_ > start_ptr_;
            } else {
                return ptr_ < start_ptr_ + size_ - 1;
            }
        }

        reference operator*() const
        {
            return *ptr_;
        }

        pointer operator->() const
        {
            return ptr_;
        }

        Iterator& operator++() noexcept
        {
            next();
            return *this;
        }

        Iterator operator++(int) noexcept
        {
            Iterator temp{*this};
            next();
            return temp;
        }

        bool operator==(const Iterator& other) const = default;
        bool operator!=(const Iterator& other) const = default;

        Iterator(const Iterator<false, Reverse>& other)
        requires Const
            : start_ptr_{other.start_ptr_}
            , ptr_{other.ptr_}
            , size_{other.size_}
        {
        }
    };

public:
    using iterator               = Iterator<false, false>;
    using const_iterator         = Iterator<true, false>;
    using reverse_iterator       = Iterator<false, true>;
    using const_reverse_iterator = Iterator<true, true>;

    iterator begin() noexcept
    {
        return iterator{buffer_, buffer_, size_};
    }
    iterator end() noexcept
    {
        return iterator{buffer_, buffer_ + size_, size_};
    }

    const_iterator cbegin() const noexcept
    {
        return const_iterator{buffer_, buffer_, size_};
    }
    const_iterator cend() const noexcept
    {
        return const_iterator{buffer_, buffer_ + size_, size_};
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator{buffer_, buffer_ + size_ - 1, size_};
    }
    reverse_iterator rend() noexcept
    {
        return reverse_iterator{buffer_, buffer_ - 1, size_};
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator{buffer_, buffer_ + size_ - 1, size_};
    }
    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator{buffer_, buffer_ - 1, size_};
    }
};
} // namespace jd
