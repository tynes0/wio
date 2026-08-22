#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "wio_version.h"

namespace wio::sdk
{
    enum class Feature : std::uint32_t
    {
        PrimitiveValues = 1u,
        ByteString = 2u,
        UnicodeText = 3u,
        Enum = 4u,
        Flagset = 5u,
        Nullable = 6u,
        Option = 7u,
        Result = 8u,
        UnitResult = 9u,
        Tuple = 10u,
        DynamicArray = 11u,
        StaticArray = 12u,
        Dictionary = 13u,
        OrderedDictionary = 14u,
        Queue = 15u,
        UnorderedSet = 16u,
        OrderedSet = 17u,
        Span = 18u,
        ByteBuffer = 19u,
        BytePool = 20u,
        Function = 21u,
        Object = 22u,
        Component = 23u,
        Interface = 24u,
        Opaque = 25u,
        Box = 26u,
        Any = 27u,
        GenericInstantiation = 28u,
        TypedAttributes = 29u,
        AsyncTask = 30u,
        ApplicationHost = 31u,
        HotReload = 32u
    };

    enum class FeatureSurface : std::uint32_t
    {
        None = 0u,
        HostValue = 1u << 0u,
        TypeMetadata = 1u << 1u,
        DynamicField = 1u << 2u,
        DirectCallAbi = 1u << 3u,
        OwnershipContract = 1u << 4u,
        ReloadAware = 1u << 5u
    };

    [[nodiscard]] constexpr FeatureSurface operator|(const FeatureSurface left, const FeatureSurface right) noexcept
    {
        return static_cast<FeatureSurface>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr FeatureSurface operator&(const FeatureSurface left, const FeatureSurface right) noexcept
    {
        return static_cast<FeatureSurface>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    struct FeatureInfo
    {
        Feature feature;
        std::string_view name;
        FeatureSurface surfaces;
        std::string_view boundary_note;

        [[nodiscard]] constexpr bool supports(const FeatureSurface surface) const noexcept
        {
            return (surfaces & surface) == surface;
        }
    };

    inline constexpr auto feature_catalog = std::to_array<FeatureInfo>({
        { Feature::PrimitiveValues, "primitive-values", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, "Stable scalar ABI." },
        { Feature::ByteString, "string", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Owned byte string; not a direct C ABI scalar." },
        { Feature::UnicodeText, "text", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Owned validated UTF-8 with code-point semantics." },
        { Feature::Enum, "enum", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, "Identity and unknown native values are preserved." },
        { Feature::Flagset, "flagset", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, "Identity and raw bit values are preserved." },
        { Feature::Nullable, "nullable", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Nested value metadata is stable; direct field mutation remains shape-dependent." },
        { Feature::Option, "Option", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Host value and generic identity are stable; exported instances remain handle-based." },
        { Feature::Result, "Result", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Host error shape mirrors std::ResultError." },
        { Feature::UnitResult, "UnitResult", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Alias of Result<Unit> on the host." },
        { Feature::Tuple, "Tuple", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Concrete generic arguments are retained in metadata." },
        { Feature::DynamicArray, "dynamic-array", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Owned vector bridge." },
        { Feature::StaticArray, "static-array", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Extent is retained in metadata." },
        { Feature::Dictionary, "dictionary", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Unordered map bridge." },
        { Feature::OrderedDictionary, "ordered-dictionary", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "Ordered map bridge." },
        { Feature::Queue, "queue", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Host queue mirrors std collection behavior." },
        { Feature::UnorderedSet, "unordered-set", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Host hash-set mirror." },
        { Feature::OrderedSet, "ordered-set", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Host ordered-set mirror." },
        { Feature::Span, "span", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, "Borrowed host span never owns its storage." },
        { Feature::ByteBuffer, "byte-buffer", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Owned cursor-based byte storage." },
        { Feature::BytePool, "byte-pool", FeatureSurface::HostValue | FeatureSurface::OwnershipContract, "Generation-checked rent/release handles." },
        { Feature::Function, "function", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, "std::function-backed dynamic field bridge." },
        { Feature::Object, "object", FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, "Owned and borrowed generation-bound handles." },
        { Feature::Component, "component", FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, "Owned and borrowed generation-bound handles." },
        { Feature::Interface, "interface", FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, "Reflected handle identity; concrete invocation requires an exported adapter." },
        { Feature::Opaque, "opaque", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DirectCallAbi, "Pointer identity only; lifetime belongs to the declared native contract." },
        { Feature::Box, "Box", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, "Move/ownership is explicit at the host boundary." },
        { Feature::Any, "any", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, "Runtime type identity is preserved; cross-module payloads require an adapter." },
        { Feature::GenericInstantiation, "generic-instantiation", FeatureSurface::TypeMetadata, "Concrete type and const argument identities are descriptor metadata, not a C++ template ABI." },
        { Feature::TypedAttributes, "typed-attributes", FeatureSurface::None, "Typed attributes are a language feature; retained host metadata is reserved for the attribute ABI milestone." },
        { Feature::AsyncTask, "async-task", FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, "Task identity is visible; non-blocking host control lands with the async ABI capability." },
        { Feature::ApplicationHost, "application-host", FeatureSurface::None, "Reserved for the application/system host ABI milestone." },
        { Feature::HotReload, "hot-reload", FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, "Top-level bindings reacquire generations; instance wrappers fail stale." }
    });

    [[nodiscard]] constexpr std::span<const FeatureInfo> features() noexcept
    {
        return feature_catalog;
    }

    [[nodiscard]] constexpr const FeatureInfo* feature_info(const Feature feature) noexcept
    {
        for (const auto& info : feature_catalog)
        {
            if (info.feature == feature)
                return &info;
        }
        return nullptr;
    }

    [[nodiscard]] constexpr std::optional<Feature> find_feature(const std::string_view name) noexcept
    {
        for (const auto& info : feature_catalog)
        {
            if (info.name == name)
                return info.feature;
        }
        return std::nullopt;
    }
}
