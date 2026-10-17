#pragma once

#include <memory>
#include <new>
#include <stdexcept>
#include <string>
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

        // Compiler-internal: the closed-cell CFG proof guarantees empty storage
        // at the producer, and the factory cannot observe/reenter this cell.
        // Placement new preserves prvalue elision; construct_at(factory()) would
        // forward through another T and reintroduce a string move.
        template <class Factory>
            requires(std::is_same_v<T, std::string> && std::is_same_v<std::invoke_result_t<Factory &&>, T>)
        void initializeFrom(Factory&& factory)
        {
            if (initialized_)
                throw std::logic_error("initialization of live Wio inline place");
            ::new (static_cast<void*>(std::addressof(storage_.value))) T(std::forward<Factory>(factory)());
            initialized_ = true;
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
