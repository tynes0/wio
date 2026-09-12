#pragma once

#include "exception.h"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace wio::wir_backend
{
    template<class T, bool = std::is_integral_v<T>> class Iterator;

    template<class T> class Iterator<T, true>
    {
    public:
        static Iterator range(T start, T end, T step, bool inclusive)
        {
            if (step == 0) throw runtime::RuntimeException("Range step cannot be zero.");
            return Iterator(start, end, step, inclusive);
        }
        bool hasNext() const
        {
            if (finished_) return false;
            return step_ > 0 ? (inclusive_ ? current_ <= end_ : current_ < end_)
                             : (inclusive_ ? current_ >= end_ : current_ > end_);
        }
        T& value() { return current_; }
        void advance()
        {
            // The mathematical next value can exceed the storage type at an
            // inclusive limit. End the iteration before integer overflow.
            if ((step_ > 0 && current_ > std::numeric_limits<T>::max() - step_) ||
                (std::is_signed_v<T> && step_ < 0 && current_ < std::numeric_limits<T>::min() - step_))
            {
                finished_ = true;
                return;
            }
            current_ = static_cast<T>(current_ + step_);
        }
    private:
        Iterator(T start, T end, T step, bool inclusive)
            : current_(start), end_(end), step_(step), inclusive_(inclusive) {}
        T current_, end_, step_;
        bool inclusive_, finished_ = false;
    };

    template<class T> class Iterator<T, false>
    {
    public:
        template<class Step = std::size_t>
        static Iterator container(T& source, Step step = 1)
        {
            if (step <= 0) throw runtime::RuntimeException("Container step must be positive.");
            return Iterator(source, static_cast<std::uintmax_t>(step));
        }
        bool hasNext() const { return position_ != source_->end(); }
        decltype(auto) value() { return *position_; }
        std::size_t index() const { return index_; }
        void advance()
        {
            for (std::uintmax_t remaining = step_; remaining != 0 && hasNext(); --remaining)
            {
                ++position_;
                ++index_;
            }
        }
    private:
        Iterator(T& source, std::uintmax_t step)
            : source_(&source), position_(source.begin()), step_(step) {}
        T* source_;
        decltype(std::declval<T&>().begin()) position_;
        std::uintmax_t step_;
        std::size_t index_ = 0;
    };
}
