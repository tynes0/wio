#pragma once

#include <type_traits>
#include <memory>

namespace wio
{
    struct packed_bool
    {
        bool b1 : 1;
        bool b2 : 1;
        bool b3 : 1;
        bool b4 : 1;
        bool b5 : 1;
        bool b6 : 1;
        bool b7 : 1;
        bool b8 : 1;
    };

    template <class _Ty>
    using ref = std::shared_ptr<_Ty>;

    template <class _Ty>
    using weak_ref = std::weak_ptr<_Ty>;

    template <class T, class... _Types>
    [[nodiscard]] ref<T> make_ref(_Types&&... args)
    {
        return std::make_shared<T, _Types...>(std::forward<_Types>(args)...);
    }

    struct no_delete{};

    template<class _Ty>
    class minimal_ref
    {
    public:
        template<class _Uty>
        friend class minimal_ref;
        constexpr minimal_ref() noexcept = default;

        constexpr minimal_ref(nullptr_t)  noexcept {}

        minimal_ref(_Ty* ptr) : m_ptr(ptr), m_ref_count(new long{ 1 }), m_no_delete(new bool{ false }) {}

        minimal_ref(_Ty* ptr, no_delete) : m_ptr(ptr), m_ref_count(new long{ 1 }), m_no_delete(new bool{ true }) {}

        template<class _Uty>
        minimal_ref(_Uty* ptr) : m_ptr(ptr), m_ref_count(new long{ 1 }), m_no_delete(new bool{ false }) {}

        template<class _Uty>
        minimal_ref(_Uty* ptr, no_delete) : m_ptr(ptr), m_ref_count(new long{ 1 }), m_no_delete(new bool{ true }) {}

        minimal_ref(const minimal_ref& sp) noexcept : m_ptr(sp.m_ptr), m_ref_count(sp.m_ref_count), m_no_delete(sp.m_no_delete)
        {
            if (m_ptr) ++(*m_ref_count);
        }

        template<class _Uty>
        minimal_ref(const minimal_ref<_Uty>& sp) noexcept : m_ptr(sp.m_ptr), m_ref_count(sp.m_ref_count), m_no_delete(sp.m_no_delete)
        {
            if (m_ptr) ++(*m_ref_count);
        }

        ~minimal_ref()
        {
            if (m_ptr)
            {
                if (--(*m_ref_count) == 0)
                {
                    if(!(*m_no_delete))
                        delete m_ptr;

                    delete m_ref_count;
                    delete m_no_delete;
                }
            }
        }

        minimal_ref& operator=(const minimal_ref& sp) noexcept
        {
            minimal_ref tmp(sp);
            tmp.swap(*this);
            return *this;
        }

        _Ty& operator*() const noexcept
        {
            return *m_ptr;
        }

        _Ty* operator->() const noexcept
        {
            return m_ptr;
        }

        _Ty* get() const noexcept
        {
            return m_ptr;
        }

        long use_count() const noexcept
        {
            if (m_ptr)
                return *m_ref_count;
            else
                return 0;
        }

        bool unique() const noexcept
        {
            return (use_count() == 1) ? true : false;
        }

        explicit operator bool() const noexcept
        {
            return (m_ptr) ? true : false;
        }

        void reset() noexcept
        {
            minimal_ref tmp{};
            tmp.swap(*this);
        }

        template<typename U>
        void reset(U* p)
        {
            minimal_ref tmp(p);
            tmp.swap(*this);
        }

        void swap(minimal_ref& sp) noexcept
        {
            std::swap(m_ptr, sp.m_ptr);
            std::swap(m_ref_count, sp.m_ref_count);
            std::swap(m_no_delete, sp.m_no_delete);
        }

        void set_delete_state(bool state)
        {
            if (m_ptr) 
                *m_no_delete = !state;
        }

    private:
        _Ty* m_ptr = nullptr;
        long* m_ref_count = nullptr;
        bool* m_no_delete = nullptr;
    };


    template<class _Ty, class _Uty>
    inline bool operator==(const minimal_ref<_Ty>& lhs, const minimal_ref<_Uty>& rhs)
    {
        return lhs.get() == rhs.get();
    }

    template<class _Ty>
    inline bool operator==(const minimal_ref<_Ty>& lhs, nullptr_t) noexcept
    {
        return !lhs;
    }

    template<class _Ty>
    inline bool operator==(nullptr_t, const minimal_ref<_Ty>& rhs) noexcept
    {
        return !rhs;
    }

    template<class _Ty, class _Uty>
    inline bool operator!=(const minimal_ref<_Ty>& lhs, const minimal_ref<_Uty>& rhs)
    {
        return lhs.get() != rhs.get();
    }

    template<class _Ty>
    inline bool operator!=(const minimal_ref<_Ty>& lhs, nullptr_t) noexcept
    {
        return bool(lhs);
    }

    template<class _Ty>
    inline bool operator!=(nullptr_t, const minimal_ref<_Ty>& rhs) noexcept
    {
        return bool(rhs);
    }

    template <class _Ty>
    constexpr void swap(minimal_ref<_Ty>& left, minimal_ref<_Ty>& right)
    {
        left.swap(right);
    }

    template <class _Ty>
    _Ty* get(const minimal_ref<_Ty>& up)
    {
        return up.get();
    }


    template <class _Ty, class ..._Args>
    minimal_ref<_Ty> make_minimal_ref(_Args&&... args)
    {
        return minimal_ref<_Ty>(new _Ty(static_cast<_Args&&>(args)...));
    }


    struct char_reference
    {
        char_reference() : m_ref(new char{}) {}
        char_reference(char ch) : m_ref(new char(ch)) {}
        char_reference(char* ptr) : m_ref(ptr) {}
        char_reference(char* ptr, no_delete) : m_ref(ptr, no_delete{}) {}
        char_reference(std::string& str, size_t index) : m_ref(&str[index], no_delete{}) {}

        char& get() { return *m_ref; }
        const char& get() const { return *m_ref; }
        void set(char ch) { *m_ref = ch; }
        void set_refer(char* chptr) { minimal_ref<char>(chptr).swap(m_ref); }
        void set_refer(char* chptr, no_delete) { minimal_ref<char>(chptr, no_delete{}).swap(m_ref); }
        bool is_valid() const { return bool(m_ref); }
        void set_delete_state(bool state) { m_ref.set_delete_state(state); }

        operator char() const { return get(); }
        operator const char&() const { return get(); }
        operator char&() { return get(); }
    private:
        minimal_ref<char> m_ref = nullptr;
    };
}
