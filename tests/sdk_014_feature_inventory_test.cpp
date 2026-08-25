#include <cassert>

#include <wio_sdk.h>

int main()
{
    using namespace wio::sdk;

    constexpr Feature changedValues[] = {
        Feature::Option,
        Feature::Result,
        Feature::UnitResult,
        Feature::Tuple,
        Feature::DynamicArray,
        Feature::StaticArray,
        Feature::Dictionary,
        Feature::OrderedDictionary,
        Feature::Queue,
        Feature::UnorderedSet,
        Feature::OrderedSet,
        Feature::Span,
        Feature::ByteBuffer
    };

    for (const auto feature : changedValues)
    {
        const auto* info = feature_info(feature);
        assert(info != nullptr);
        assert(info->is_supported());
        assert(info->supports(FeatureSurface::HostValue));
        assert(info->supports(FeatureSurface::TypeMetadata));
        assert(info->supports(FeatureSurface::DynamicField));
    }

    assert(feature_info(Feature::GenericInstantiation)->is_partial());
    assert(feature_info(Feature::GenericInstantiation)->supports(FeatureSurface::TypeMetadata));
    assert(!feature_info(Feature::GenericInstantiation)->supports(FeatureSurface::DynamicField));
    assert(feature_info(Feature::TypedAttributes)->is_supported());
    assert(feature_info(Feature::TypedAttributes)->supports(FeatureSurface::TypeMetadata));
    assert(feature_info(Feature::ApplicationHost)->is_supported());
    assert(feature_info(Feature::ApplicationHost)->supports(FeatureSurface::OwnershipContract));
    assert(feature_info(Feature::AsyncTask)->is_supported());
    assert(feature_info(Feature::AsyncTask)->supports(FeatureSurface::HostValue));
    assert(feature_info(Feature::AsyncTask)->supports(FeatureSurface::ReloadAware));
    assert(feature_info(Feature::BytePool)->is_partial());
    assert(find_feature("byte-buffer") == Feature::ByteBuffer);

    return 0;
}
