#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace wio::meta
{
    template <typename... Ts>
    struct TypePackView
    {
        static constexpr std::size_t size = sizeof...(Ts);

        constexpr std::size_t Size() const noexcept
        {
            return size;
        }

        template <std::size_t I>
        using At = std::tuple_element_t<I, std::tuple<Ts...>>;

        template <typename T>
        static constexpr bool Contains() noexcept
        {
            return (std::is_same_v<T, Ts> || ...);
        }
    };

    template <typename... Ts>
    class ValuePackView
    {
    public:
        static constexpr std::size_t size = sizeof...(Ts);

        constexpr explicit ValuePackView(Ts... values)
            : values_(std::forward<Ts>(values)...)
        {
        }

        constexpr std::size_t Size() const noexcept
        {
            return size;
        }

        template <std::size_t I>
        constexpr decltype(auto) Get()
        {
            return std::get<I>(values_);
        }

        template <std::size_t I>
        constexpr decltype(auto) Get() const
        {
            return std::get<I>(values_);
        }

        template <std::size_t DistanceFromEnd = 1>
        constexpr decltype(auto) GetFromEnd()
        {
            static_assert(DistanceFromEnd > 0 && DistanceFromEnd <= size, "Pack index is out of range.");
            return std::get<size - DistanceFromEnd>(values_);
        }

        template <std::size_t DistanceFromEnd = 1>
        constexpr decltype(auto) GetFromEnd() const
        {
            static_assert(DistanceFromEnd > 0 && DistanceFromEnd <= size, "Pack index is out of range.");
            return std::get<size - DistanceFromEnd>(values_);
        }

        constexpr decltype(auto) First()
        {
            return Get<0>();
        }

        constexpr decltype(auto) First() const
        {
            return Get<0>();
        }

        constexpr decltype(auto) Last()
        {
            return GetFromEnd<>();
        }

        constexpr decltype(auto) Last() const
        {
            return GetFromEnd<>();
        }

        constexpr ValuePackView AsView() const
        {
            return *this;
        }

        template <typename T>
        constexpr auto ToStaticArray() const
        {
            return toStaticArrayImpl<T>(std::index_sequence_for<Ts...>{});
        }

    private:
        template <typename T, std::size_t... Is>
        constexpr auto toStaticArrayImpl(std::index_sequence<Is...>) const -> std::array<T, sizeof...(Ts)>
        {
            return { static_cast<T>(std::get<Is>(values_))... };
        }

        std::tuple<Ts...> values_;
    };

    template <typename... Ts>
    class PackStorage
    {
    public:
        static constexpr std::size_t size = sizeof...(Ts);

        constexpr PackStorage() = default;

        constexpr explicit PackStorage(Ts... values)
            : values_(std::forward<Ts>(values)...)
        {
        }

        template <typename... Us>
        constexpr explicit PackStorage(const ValuePackView<Us...>& values)
        {
            assign(values);
        }

        constexpr std::size_t Size() const noexcept
        {
            return size;
        }

        template <std::size_t I>
        constexpr decltype(auto) Get()
        {
            return std::get<I>(values_);
        }

        template <std::size_t I>
        constexpr decltype(auto) Get() const
        {
            return std::get<I>(values_);
        }

        template <std::size_t DistanceFromEnd = 1>
        constexpr decltype(auto) GetFromEnd()
        {
            static_assert(DistanceFromEnd > 0 && DistanceFromEnd <= size, "Pack index is out of range.");
            return std::get<size - DistanceFromEnd>(values_);
        }

        template <std::size_t DistanceFromEnd = 1>
        constexpr decltype(auto) GetFromEnd() const
        {
            static_assert(DistanceFromEnd > 0 && DistanceFromEnd <= size, "Pack index is out of range.");
            return std::get<size - DistanceFromEnd>(values_);
        }

        constexpr decltype(auto) First()
        {
            return Get<0>();
        }

        constexpr decltype(auto) First() const
        {
            return Get<0>();
        }

        constexpr decltype(auto) Last()
        {
            return GetFromEnd<>();
        }

        constexpr decltype(auto) Last() const
        {
            return GetFromEnd<>();
        }

        constexpr auto AsView()
        {
            return asViewImpl(std::index_sequence_for<Ts...>{});
        }

        constexpr auto AsView() const
        {
            return asConstViewImpl(std::index_sequence_for<Ts...>{});
        }

        template <typename T>
        constexpr auto ToStaticArray() const
        {
            return AsView().template ToStaticArray<T>();
        }

        template <typename... Us>
        constexpr PackStorage& operator=(const ValuePackView<Us...>& values)
        {
            assign(values);
            return *this;
        }

    private:
        template <typename... Us>
        constexpr void assign(const ValuePackView<Us...>& values)
        {
            static_assert(sizeof...(Us) == sizeof...(Ts), "Pack size mismatch.");
            assignImpl(values, std::index_sequence_for<Ts...>{});
        }

        template <typename... Us, std::size_t... Is>
        constexpr void assignImpl(const ValuePackView<Us...>& values, std::index_sequence<Is...>)
        {
            ((std::get<Is>(values_) = values.template Get<Is>()), ...);
        }

        template <std::size_t... Is>
        constexpr auto asViewImpl(std::index_sequence<Is...>)
        {
            return ValuePackView<Ts&...>(std::get<Is>(values_)...);
        }

        template <std::size_t... Is>
        constexpr auto asConstViewImpl(std::index_sequence<Is...>) const
        {
            return ValuePackView<const Ts&...>(std::get<Is>(values_)...);
        }

        std::tuple<Ts...> values_{};
    };

    template <typename T, typename... Ts>
    constexpr auto PackToStaticArray(Ts... values) -> std::array<T, sizeof...(Ts)>
    {
        return { static_cast<T>(std::forward<Ts>(values))... };
    }

    template <typename... Ts>
    constexpr auto MakePackView(Ts... values)
    {
        return ValuePackView<Ts...>(std::forward<Ts>(values)...);
    }

    template <typename... Ts>
    constexpr auto MakePackStorage(Ts... values)
    {
        return PackStorage<Ts...>(std::forward<Ts>(values)...);
    }

    template <typename... Ts>
    inline constexpr std::size_t TypeCount = sizeof...(Ts);

    template <typename T, typename... Ts>
    inline constexpr bool Contains = (std::is_same_v<T, Ts> || ...);

    template <template <typename> typename Trait, typename... Ts>
    inline constexpr bool All = (Trait<Ts>::value && ...);

    template <template <typename> typename Trait, typename... Ts>
    inline constexpr bool Any = (Trait<Ts>::value || ...);

    namespace detail
    {
        template <typename... Ts>
        struct TypeList
        {
        };

        template <typename LeftList, typename RightList>
        struct Concat;

        template <typename... Left, typename... Right>
        struct Concat<TypeList<Left...>, TypeList<Right...>>
        {
            using type = TypeList<Left..., Right...>;
        };

        template <std::size_t Count, typename Accumulated, typename Remaining>
        struct TakeImpl;

        template <std::size_t Count, typename... Accumulated, typename Head, typename... Tail>
        struct TakeImpl<Count, TypeList<Accumulated...>, TypeList<Head, Tail...>>
            : TakeImpl<Count - 1, TypeList<Accumulated..., Head>, TypeList<Tail...>>
        {
        };

        template <typename... Accumulated, typename... Remaining>
        struct TakeImpl<0, TypeList<Accumulated...>, TypeList<Remaining...>>
        {
            using type = TypeList<Accumulated...>;
        };

        template <std::size_t Count, typename... Accumulated>
        struct TakeImpl<Count, TypeList<Accumulated...>, TypeList<>>
        {
            using type = TypeList<Accumulated...>;
        };

        template <std::size_t Count, typename Remaining>
        struct DropImpl;

        template <std::size_t Count, typename Head, typename... Tail>
        struct DropImpl<Count, TypeList<Head, Tail...>>
            : DropImpl<Count - 1, TypeList<Tail...>>
        {
        };

        template <typename... Remaining>
        struct DropImpl<0, TypeList<Remaining...>>
        {
            using type = TypeList<Remaining...>;
        };

        template <std::size_t Count>
        struct DropImpl<Count, TypeList<>>
        {
            using type = TypeList<>;
        };

        template <template <typename> typename Mapper, typename TypeListValue>
        struct MapTypesImpl;

        template <template <typename> typename Mapper, typename... Ts>
        struct MapTypesImpl<Mapper, TypeList<Ts...>>
        {
            using type = TypeList<typename Mapper<Ts>::type...>;
        };

        template <typename LeftList, typename RightList>
        struct ZipImpl;

        template <>
        struct ZipImpl<TypeList<>, TypeList<>>
        {
            using type = TypeList<>;
        };

        template <typename... Right>
        struct ZipImpl<TypeList<>, TypeList<Right...>>
        {
            using type = TypeList<>;
        };

        template <typename... Left>
        struct ZipImpl<TypeList<Left...>, TypeList<>>
        {
            using type = TypeList<>;
        };

        template <typename LeftHead, typename... LeftTail, typename RightHead, typename... RightTail>
        struct ZipImpl<TypeList<LeftHead, LeftTail...>, TypeList<RightHead, RightTail...>>
        {
            using type = typename Concat<
                TypeList<std::pair<LeftHead, RightHead>>,
                typename ZipImpl<TypeList<LeftTail...>, TypeList<RightTail...>>::type
            >::type;
        };
    }

    template <std::size_t Count, typename... Ts>
    struct Take
    {
        using type = typename detail::TakeImpl<Count, detail::TypeList<>, detail::TypeList<Ts...>>::type;
    };

    template <std::size_t Count, typename... Ts>
    struct Drop
    {
        using type = typename detail::DropImpl<Count, detail::TypeList<Ts...>>::type;
    };

    template <template <typename> typename Mapper, typename... Ts>
    struct MapTypes
    {
        using type = typename detail::MapTypesImpl<Mapper, detail::TypeList<Ts...>>::type;
    };

    template <typename... Ts>
    struct Zip;

    template <typename... Left, typename... Right>
    struct Zip<detail::TypeList<Left...>, detail::TypeList<Right...>>
    {
        using type = typename detail::ZipImpl<detail::TypeList<Left...>, detail::TypeList<Right...>>::type;
    };
}
