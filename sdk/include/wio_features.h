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

    enum class FeatureSupport : std::uint8_t
    {
        Supported,
        Partial,
        Deferred
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
        FeatureSupport support;
        std::string_view boundary_note;

        [[nodiscard]] constexpr bool supports(const FeatureSurface surface) const noexcept
        {
            return (surfaces & surface) == surface;
        }

        [[nodiscard]] constexpr bool is_supported() const noexcept { return support == FeatureSupport::Supported; }
        [[nodiscard]] constexpr bool is_partial() const noexcept { return support == FeatureSupport::Partial; }
        [[nodiscard]] constexpr bool is_deferred() const noexcept { return support == FeatureSupport::Deferred; }
    };

    inline constexpr auto feature_catalog = std::to_array<FeatureInfo>({
        { Feature::PrimitiveValues, "primitive-values", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, FeatureSupport::Supported, "Stable scalar ABI." },
        { Feature::ByteString, "string", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned byte string; not a direct C ABI scalar." },
        { Feature::UnicodeText, "text", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned validated UTF-8 with code-point semantics." },
        { Feature::Enum, "enum", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, FeatureSupport::Supported, "Identity and unknown native values are preserved." },
        { Feature::Flagset, "flagset", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::DirectCallAbi, FeatureSupport::Supported, "Identity and raw bit values are preserved." },
        { Feature::Nullable, "nullable", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, FeatureSupport::Partial, "Nested value metadata is stable; direct field mutation remains shape-dependent." },
        { Feature::Option, "Option", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive field bridge; unsupported nested payloads fail Wio analysis." },
        { Feature::Result, "Result", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive field bridge preserves the complete error record." },
        { Feature::UnitResult, "UnitResult", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Unit and Result<Unit> use dedicated value descriptors." },
        { Feature::Tuple, "Tuple", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Arity and every recursively bridged slot are checked." },
        { Feature::DynamicArray, "dynamic-array", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive vector bridge." },
        { Feature::StaticArray, "static-array", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive array bridge with retained extent." },
        { Feature::Dictionary, "dictionary", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive unordered-map bridge." },
        { Feature::OrderedDictionary, "ordered-dictionary", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned recursive ordered-map bridge." },
        { Feature::Queue, "queue", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned queue bridge preserves order." },
        { Feature::UnorderedSet, "unordered-set", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned set bridge preserves membership semantics." },
        { Feature::OrderedSet, "ordered-set", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned set bridge preserves ordering." },
        { Feature::Span, "span", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::OwnershipContract, FeatureSupport::Supported, "WioSpanRange crosses the module; WioSpan<T> borrows host-owned storage." },
        { Feature::ByteBuffer, "byte-buffer", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "Owned bytes, reserved capacity, and cursor position round-trip." },
        { Feature::BytePool, "byte-pool", FeatureSurface::HostValue | FeatureSurface::OwnershipContract, FeatureSupport::Partial, "Host and Wio pools use generation-checked handles; pool identity does not cross modules." },
        { Feature::Function, "function", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DynamicField, FeatureSupport::Supported, "std::function-backed dynamic field bridge." },
        { Feature::Object, "object", FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, FeatureSupport::Supported, "Owned and borrowed generation-bound handles." },
        { Feature::Component, "component", FeatureSurface::TypeMetadata | FeatureSurface::DynamicField | FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, FeatureSupport::Supported, "Owned and borrowed generation-bound handles." },
        { Feature::Interface, "interface", FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, FeatureSupport::Partial, "Reflected handle identity; concrete invocation requires an exported adapter." },
        { Feature::Opaque, "opaque", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::DirectCallAbi, FeatureSupport::Supported, "Pointer identity only; lifetime belongs to the declared native contract." },
        { Feature::Box, "Box", FeatureSurface::HostValue | FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, FeatureSupport::Partial, "Host ownership value exists; a general module payload bridge is deferred." },
        { Feature::Any, "any", FeatureSurface::HostValue | FeatureSurface::TypeMetadata, FeatureSupport::Partial, "Runtime type identity is preserved; cross-module payloads require an adapter." },
        { Feature::GenericInstantiation, "generic-instantiation", FeatureSurface::TypeMetadata, FeatureSupport::Partial, "ABI v8 describes concrete type and const values; only exported specializations are host-addressable." },
        { Feature::TypedAttributes, "typed-attributes", FeatureSurface::None, FeatureSupport::Deferred, "Retained host metadata is reserved for the attribute ABI milestone." },
        { Feature::AsyncTask, "async-task", FeatureSurface::TypeMetadata | FeatureSurface::OwnershipContract, FeatureSupport::Partial, "Task identity is visible; non-blocking host control lands with the async ABI capability." },
        { Feature::ApplicationHost, "application-host", FeatureSurface::None, FeatureSupport::Deferred, "Reserved for the application/system host ABI milestone." },
        { Feature::HotReload, "hot-reload", FeatureSurface::OwnershipContract | FeatureSurface::ReloadAware, FeatureSupport::Supported, "Top-level bindings reacquire generations; instance wrappers fail stale." }
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
