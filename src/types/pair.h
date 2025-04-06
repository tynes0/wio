#pragma once

#include <type_traits>
#include <utility>

namespace wio
{
	template <class _Ty1, class _Ty2 = _Ty1>
	struct pair
	{
		using first_type = _Ty1;
		using second_type = _Ty2;
		first_type first;
		second_type second;

		constexpr static bool nothrow_swappable = (std::is_nothrow_swappable_v<_Ty1> && std::is_nothrow_swappable_v<_Ty2>);

		template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
		constexpr explicit(!std::conjunction_v<std::is_default_constructible<_Uty1>, std::is_default_constructible<_Uty2>>)
			pair() noexcept : first(), second() {}

		template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
		constexpr explicit(!std::conjunction_v<std::is_convertible<const _Uty1&, _Uty1>, std::is_convertible<const _Uty2&, _Uty2>>)
			pair(const _Ty1& first_in, const _Ty2& second_in) noexcept : first(first_in), second(second_in) {}

		template <class _Other1, class _Other2>
		constexpr explicit(!std::conjunction_v<std::is_convertible<const _Other1&, _Ty1>, std::is_convertible<const _Other2&, _Ty2>>)
			pair(const pair<_Other1, _Other2>& other) noexcept : first(other.first), second(other.second) {}

		pair(const pair&) = default;
		pair(pair&&) = default;

		pair& operator=(const volatile pair&) = delete;

		constexpr pair& operator=(const pair& _Right)
		{
			first = _Right.first;
			second = _Right.second;
			return *this;
		}

		constexpr bool operator==(const pair& _Right) const
		{
			return first == _Right.first && second == _Right.second;
		}

		constexpr bool operator!=(const pair& _Right) const
		{
			return !(*this == _Right);
		}

		constexpr void swap(pair<_Ty1, _Ty2>& _Other)
		{
			if (this != std::addressof(_Other))
			{
				std::swap(first, _Other.first);
				std::swap(second, _Other.second);
			}
		}

		constexpr bool types_are_the_same() const noexcept
		{
			return std::is_same_v<_Ty1, _Ty2>;
		}
	};


	template <class A, class B>
	constexpr pair<std::unwrap_reference_t<A>, std::unwrap_reference_t<B>> make_pair(A&& first, B&& second)
		noexcept(std::is_nothrow_constructible_v<std::unwrap_reference_t<A>, A>&& std::is_nothrow_constructible_v<std::unwrap_reference_t<B>, B>)
	{
		return pair<std::unwrap_reference_t<A>, std::unwrap_reference_t<B>>{ std::forward<A>(first), std::forward<B>(second) };
	}

	template <class A, class B>
	void swap(pair<A, B>& a, pair<A, B>& b) noexcept(pair<A, B>::nothrow_swappable)
	{
		a.swap(b);
	}
}