#pragma once

#include "ref.h"
#include "text.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace wio::runtime
{
    enum class ReflectedTypeKind : std::uint8_t
    {
        unknown = 0,
        primitive_type = 1,
        enum_type = 2,
        flagset_type = 3,
        component_type = 4,
        object_type = 5,
        interface_type = 6,
        array_type = 7,
        dictionary_type = 8,
        reference_type = 9
    };

    template <typename T>
    struct TypeReflection
    {
        static constexpr std::string_view Name = "<unknown>";
        static constexpr ReflectedTypeKind Kind =
            std::is_arithmetic_v<T> || std::is_same_v<T, std::string>
                ? ReflectedTypeKind::primitive_type
                : ReflectedTypeKind::unknown;
        static constexpr std::array<std::string_view, 0> FieldNames{};
        static constexpr std::array<std::string_view, 0> FieldTypes{};
        static constexpr std::array<std::string_view, 0> FieldAccess{};
        static constexpr std::array<std::string_view, 0> MethodNames{};
        static constexpr std::array<std::string_view, 0> MethodSignatures{};
        static constexpr std::array<std::string_view, 0> MethodAccess{};
        static constexpr std::array<std::string_view, 0> BaseTypes{};
    };

    template <>
    struct TypeReflection<Text>
    {
        static constexpr std::string_view Name = "text";
        static constexpr ReflectedTypeKind Kind = ReflectedTypeKind::primitive_type;
        static constexpr std::array<std::string_view, 0> FieldNames{};
        static constexpr std::array<std::string_view, 0> FieldTypes{};
        static constexpr std::array<std::string_view, 0> FieldAccess{};
        static constexpr std::array<std::string_view, 0> MethodNames{};
        static constexpr std::array<std::string_view, 0> MethodSignatures{};
        static constexpr std::array<std::string_view, 0> MethodAccess{};
        static constexpr std::array<std::string_view, 0> BaseTypes{};
    };

    template <typename T>
    struct TypeReflection<Ref<T>> : TypeReflection<T>
    {
        static constexpr ReflectedTypeKind StorageKind = ReflectedTypeKind::reference_type;
    };

    template <typename T, typename TAllocator>
    struct TypeReflection<std::vector<T, TAllocator>>
    {
        static constexpr std::string_view Name = "array";
        static constexpr ReflectedTypeKind Kind = ReflectedTypeKind::array_type;
        static constexpr std::array<std::string_view, 0> FieldNames{};
        static constexpr std::array<std::string_view, 0> FieldTypes{};
        static constexpr std::array<std::string_view, 0> FieldAccess{};
        static constexpr std::array<std::string_view, 0> MethodNames{};
        static constexpr std::array<std::string_view, 0> MethodSignatures{};
        static constexpr std::array<std::string_view, 0> MethodAccess{};
        static constexpr std::array<std::string_view, 0> BaseTypes{};
    };

    template <typename K, typename V, typename... Rest>
    struct TypeReflection<std::unordered_map<K, V, Rest...>>
    {
        static constexpr std::string_view Name = "Dict";
        static constexpr ReflectedTypeKind Kind = ReflectedTypeKind::dictionary_type;
        static constexpr std::array<std::string_view, 0> FieldNames{};
        static constexpr std::array<std::string_view, 0> FieldTypes{};
        static constexpr std::array<std::string_view, 0> FieldAccess{};
        static constexpr std::array<std::string_view, 0> MethodNames{};
        static constexpr std::array<std::string_view, 0> MethodSignatures{};
        static constexpr std::array<std::string_view, 0> MethodAccess{};
        static constexpr std::array<std::string_view, 0> BaseTypes{};
    };

    template <typename K, typename V, typename... Rest>
    struct TypeReflection<std::map<K, V, Rest...>>
    {
        static constexpr std::string_view Name = "Tree";
        static constexpr ReflectedTypeKind Kind = ReflectedTypeKind::dictionary_type;
        static constexpr std::array<std::string_view, 0> FieldNames{};
        static constexpr std::array<std::string_view, 0> FieldTypes{};
        static constexpr std::array<std::string_view, 0> FieldAccess{};
        static constexpr std::array<std::string_view, 0> MethodNames{};
        static constexpr std::array<std::string_view, 0> MethodSignatures{};
        static constexpr std::array<std::string_view, 0> MethodAccess{};
        static constexpr std::array<std::string_view, 0> BaseTypes{};
    };

    template <typename T>
    [[nodiscard]] inline std::string ReflectedTypeName()
    {
        if constexpr (TypeReflection<T>::Name != std::string_view("<unknown>"))
        {
            return std::string(TypeReflection<T>::Name);
        }
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, char>) return "char";
        else if constexpr (std::is_same_v<T, signed char>) return "i8";
        else if constexpr (std::is_same_v<T, unsigned char>) return "u8";
        else if constexpr (std::is_same_v<T, std::int16_t>) return "i16";
        else if constexpr (std::is_same_v<T, std::uint16_t>) return "u16";
        else if constexpr (std::is_same_v<T, std::int32_t>) return "i32";
        else if constexpr (std::is_same_v<T, std::uint32_t>) return "u32";
        else if constexpr (std::is_same_v<T, std::int64_t>) return "i64";
        else if constexpr (std::is_same_v<T, std::uint64_t>) return "u64";
        else if constexpr (std::is_same_v<T, float>) return "f32";
        else if constexpr (std::is_same_v<T, double>) return "f64";
        else
        {
            return "<unknown>";
        }
    }

    template <typename T>
    [[nodiscard]] constexpr ReflectedTypeKind ReflectedKind() noexcept
    {
        return TypeReflection<T>::Kind;
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t ReflectedSize() noexcept
    {
        return sizeof(T);
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t ReflectedAlignment() noexcept
    {
        return alignof(T);
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedFieldNames()
    {
        return { TypeReflection<T>::FieldNames.begin(), TypeReflection<T>::FieldNames.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedFieldTypes()
    {
        return { TypeReflection<T>::FieldTypes.begin(), TypeReflection<T>::FieldTypes.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedFieldAccess()
    {
        return { TypeReflection<T>::FieldAccess.begin(), TypeReflection<T>::FieldAccess.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodNames()
    {
        return { TypeReflection<T>::MethodNames.begin(), TypeReflection<T>::MethodNames.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodSignatures()
    {
        return { TypeReflection<T>::MethodSignatures.begin(), TypeReflection<T>::MethodSignatures.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodAccess()
    {
        return { TypeReflection<T>::MethodAccess.begin(), TypeReflection<T>::MethodAccess.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodBehaviorAttributeNames()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorAttributeNames; })
            return { TypeReflection<T>::MethodBehaviorAttributeNames.begin(), TypeReflection<T>::MethodBehaviorAttributeNames.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodBehaviorProcessorTypes()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorProcessorTypes; })
            return { TypeReflection<T>::MethodBehaviorProcessorTypes.begin(), TypeReflection<T>::MethodBehaviorProcessorTypes.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodBehaviorPhases()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorPhases; })
            return { TypeReflection<T>::MethodBehaviorPhases.begin(), TypeReflection<T>::MethodBehaviorPhases.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodBehaviorHooks()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorHooks; })
            return { TypeReflection<T>::MethodBehaviorHooks.begin(), TypeReflection<T>::MethodBehaviorHooks.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedMethodBehaviorModes()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorModes; })
            return { TypeReflection<T>::MethodBehaviorModes.begin(), TypeReflection<T>::MethodBehaviorModes.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::size_t> ReflectedMethodBehaviorOffsets()
    {
        if constexpr (requires { TypeReflection<T>::MethodBehaviorOffsets; })
            return { TypeReflection<T>::MethodBehaviorOffsets.begin(), TypeReflection<T>::MethodBehaviorOffsets.end() };
        return { 0u };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedBaseTypes()
    {
        return { TypeReflection<T>::BaseTypes.begin(), TypeReflection<T>::BaseTypes.end() };
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedGenericParameterNames()
    {
        if constexpr (requires { TypeReflection<T>::_WIOGenericParameterNames(); })
            return TypeReflection<T>::_WIOGenericParameterNames();
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedGenericArguments()
    {
        if constexpr (requires { TypeReflection<T>::_WIOGenericArguments(); })
            return TypeReflection<T>::_WIOGenericArguments();
        return {};
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t ReflectedFieldCount() noexcept
    {
        return TypeReflection<T>::FieldNames.size();
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t ReflectedMethodCount() noexcept
    {
        return TypeReflection<T>::MethodNames.size();
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributes()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributes; })
            return { TypeReflection<T>::TypeAttributes.begin(), TypeReflection<T>::TypeAttributes.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeNames()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeNames; })
            return { TypeReflection<T>::TypeAttributeNames.begin(), TypeReflection<T>::TypeAttributeNames.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::uint64_t> ReflectedTypeAttributeStableIds()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeStableIds; })
            return { TypeReflection<T>::TypeAttributeStableIds.begin(), TypeReflection<T>::TypeAttributeStableIds.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeRetentions()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeRetentions; })
            return { TypeReflection<T>::TypeAttributeRetentions.begin(), TypeReflection<T>::TypeAttributeRetentions.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeOrigins()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeOrigins; })
            return { TypeReflection<T>::TypeAttributeOrigins.begin(), TypeReflection<T>::TypeAttributeOrigins.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeArgumentNames()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeArgumentNames; })
            return { TypeReflection<T>::TypeAttributeArgumentNames.begin(), TypeReflection<T>::TypeAttributeArgumentNames.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeArgumentTypes()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeArgumentTypes; })
            return { TypeReflection<T>::TypeAttributeArgumentTypes.begin(), TypeReflection<T>::TypeAttributeArgumentTypes.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedTypeAttributeArgumentValues()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeArgumentValues; })
            return { TypeReflection<T>::TypeAttributeArgumentValues.begin(), TypeReflection<T>::TypeAttributeArgumentValues.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::uint8_t> ReflectedTypeAttributeArgumentUsedDefaults()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeArgumentUsedDefaults; })
            return { TypeReflection<T>::TypeAttributeArgumentUsedDefaults.begin(), TypeReflection<T>::TypeAttributeArgumentUsedDefaults.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::size_t> ReflectedTypeAttributeArgumentOffsets()
    {
        if constexpr (requires { TypeReflection<T>::TypeAttributeArgumentOffsets; })
            return { TypeReflection<T>::TypeAttributeArgumentOffsets.begin(), TypeReflection<T>::TypeAttributeArgumentOffsets.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::string> ReflectedFieldAttributeNames()
    {
        if constexpr (requires { TypeReflection<T>::FieldAttributeNames; })
            return { TypeReflection<T>::FieldAttributeNames.begin(), TypeReflection<T>::FieldAttributeNames.end() };
        return {};
    }

    template <typename T>
    [[nodiscard]] inline std::vector<std::size_t> ReflectedFieldAttributeOffsets()
    {
        if constexpr (requires { TypeReflection<T>::FieldAttributeOffsets; })
            return { TypeReflection<T>::FieldAttributeOffsets.begin(), TypeReflection<T>::FieldAttributeOffsets.end() };
        return {};
    }
}
