#pragma once

#include <type_traits>

#include "../base/exception.h"

#include <memory>


namespace wio
{
    class bad_any_cast : public exception
    {
    public:
        const char* what() const override
        {
            return "wio::bad_any_cast: failed conversion using wio::any_cast";
        }
    };

    namespace detail_any
    {
        class placeholder
        {
        public:
            virtual ~placeholder() {}
            virtual const type_info& type() const noexcept = 0;
        };
    }

    class any
    {
    public:
        constexpr any() noexcept : m_content(nullptr) {}

        template <class _Ty>
        any(const _Ty& value) : m_content(new holder<std::remove_cv_t<std::decay_t<const _Ty>>>(value)) {}

        any(const any& right) : m_content(right.m_content ? right.m_content->clone() : nullptr) {}

        any(any&& right) noexcept : m_content(right.m_content)
        {
            right.m_content = nullptr;
        }

        template <class _Ty>
        any(_Ty&& value, std::enable_if_t<!std::is_same_v<any&, _Ty>>* = 0, std::enable_if_t<!std::is_const_v<_Ty>>* = 0)
            : m_content(new holder<std::decay_t<_Ty>>(std::forward<_Ty>(value))) {
        }

        ~any()
        {
            if(m_content)
                delete m_content;
        }


        any& swap(any& right) noexcept
        {
            if (std::addressof(right) != this)
            {
                placeholder* temp = m_content;
                m_content = right.m_content;
                right.m_content = temp;
                
            }
            return *this;
        }

        any& operator=(const any& right)
        {
            any(right).swap(*this);
            return *this;
        }

        any& operator=(any&& right) noexcept
        {
            right.swap(*this);
            any().swap(right);
            return *this;
        }

        template <class _Ty>
        any& operator=(_Ty&& right)
        {
            any(std::forward<_Ty>(right)).swap(*this);
            return *this;
        }

        bool empty() const noexcept
        {
            return !m_content;
        }

        void clear() noexcept
        {
            any().swap(*this);
        }

        const std::type_info& type() const noexcept
        {
            return m_content ? m_content->type() : typeid(void);
        }

    private:
        class placeholder : public detail_any::placeholder
        {
        public:
            virtual placeholder* clone() const = 0;
        };

        template <class _Ty>
        class holder final : public placeholder
        {
        public:
            holder(const _Ty& value) : held(value) {}

            holder(_Ty&& value) :held(static_cast<_Ty&&>(value)) {}

            virtual const std::type_info& type() const noexcept override
            {
                return typeid(_Ty);
            }

            placeholder* clone() const override
            {
                return new holder(held);
            }

            _Ty held;
        private:
            holder& operator=(const holder&);
        };

    private:
        template <class _Ty>
        friend _Ty* unsafe_any_cast(any*) noexcept;
        placeholder* m_content;
    };

    inline void swap(any& left, any& right) noexcept
    {
        left.swap(right);
    }

    template <class _Ty>
    inline _Ty* unsafe_any_cast(any* operand) noexcept
    {
        return std::addressof(static_cast<any::holder<_Ty>*>(operand->m_content)->held);
    }

    template <class _Ty>
    inline const _Ty* unsafe_any_cast(const any* operand) noexcept
    {
        return unsafe_any_cast<_Ty>(const_cast<any*>(operand));
    }

    template <class _Ty>
    _Ty* any_cast(any* operand) noexcept
    {
        return operand && operand->type() == typeid(_Ty) ? unsafe_any_cast<std::remove_cv_t<_Ty>>(operand) : 0;
    }

    template <class _Ty>
    inline const _Ty* any_cast(const any* operand) noexcept
    {
        return any_cast<_Ty>(const_cast<any*>(operand));
    }

    template <class _Ty>
    _Ty any_cast(any& operand)
    {
        using nonref = std::remove_reference_t<_Ty>;

        nonref* result = any_cast<nonref>(std::addressof(operand));
        if (!result)
            throw(bad_any_cast());
        return static_cast<std::conditional_t<std::is_reference_v<_Ty>, _Ty, std::add_lvalue_reference_t<_Ty>>>(*result);
    }

    template <class _Ty>
    inline _Ty any_cast(const any& operand)
    {
        return any_cast<const std::remove_reference_t<_Ty>&>(const_cast<any&>(operand));
    }

    template <class _Ty>
    inline _Ty any_cast(any&& operand)
    {
        static_assert(std::is_rvalue_reference_v<_Ty&&> || std::is_const_v<std::remove_reference_t<_Ty>>, "any_cast shall not be used for getting nonconst references to temporary objects");
        return any_cast<_Ty>(operand);
    }
}