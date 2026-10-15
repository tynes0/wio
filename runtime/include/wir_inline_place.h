#pragma once

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace wio::wir_backend
{
    // Only the backend's closed-use storage proof may select this cell. It has
    // no shared identity: references, captures and escaping cells use Place.
    template <class T> class InlinePlace final
    {
        static_assert(std::is_nothrow_destructible_v<T>);

    public:
        InlinePlace() noexcept = default;
        InlinePlace(const InlinePlace&) = delete;
        InlinePlace& operator=(const InlinePlace&) = delete;

        InlinePlace(InlinePlace&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            if (other.initialized_)
            {
                std::construct_at(&storage_.value, std::move(other.storage_.value));
                initialized_ = true;
                other.clear();
            }
        }

        InlinePlace& operator=(InlinePlace&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            if (this != &other)
            {
                clear();
                if (other.initialized_)
                {
                    std::construct_at(&storage_.value, std::move(other.storage_.value));
                    initialized_ = true;
                    other.clear();
                }
            }
            return *this;
        }

        ~InlinePlace()
        {
            clear();
        }

        static InlinePlace local()
        {
            return {};
        }

        T& read()
        {
            if (!initialized_)
                throw std::runtime_error("read from uninitialized Wio place");
            return storage_.value;
        }

        const T& read() const
        {
            if (!initialized_)
                throw std::runtime_error("read from uninitialized Wio place");
            return storage_.value;
        }

        void write(T value)
        {
            if (initialized_)
                storage_.value = std::move(value);
            else
            {
                std::construct_at(&storage_.value, std::move(value));
                initialized_ = true;
            }
        }

        void clear() noexcept
        {
            if (initialized_)
            {
                initialized_ = false;
                std::destroy_at(&storage_.value);
            }
        }

        T take()
        {
            T value = std::move(read());
            clear();
            return value;
        }

    private:
        union Storage
        {
            Storage() noexcept : empty{}
            {
            }

            ~Storage()
            {
            }

            char empty;
            T value;
        } storage_;

        bool initialized_ = false;
    };
} // namespace wio::wir_backend
