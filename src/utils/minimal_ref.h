#pragma once

#include <type_traits>
#include <utility>

struct no_delete {};

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
                if (!(*m_no_delete))
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