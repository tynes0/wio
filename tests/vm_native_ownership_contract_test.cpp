#include "native_bridge_internal.h"
#include <wir_abi.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace contract
{
    constexpr std::uint64_t PodId = 801;
    constexpr std::uint64_t FieldId = 802;
    constexpr std::uint64_t RootId = 803;
    struct Pod
    {
        std::int32_t field = 7;
        std::int32_t hidden = 19;
    };
    using Root = std::vector<Pod>;
} // namespace contract
namespace wio::wir_backend::abi
{
    template <> struct NativePodProjectionAccess<contract::Pod>
    {
        static void* resolve(void* storage, std::uint64_t node, std::uint64_t target, std::uint64_t offset,
                             std::uint64_t size, const std::type_info& requested) noexcept
        {
            if (storage && node == contract::PodId && target == contract::FieldId &&
                offset == offsetof(contract::Pod, field) && size == sizeof(std::int32_t) &&
                requested == typeid(std::int32_t))
                return &static_cast<contract::Pod*>(storage)->field;
            return nullptr;
        }
    };
    template <> struct NativeContainerProjectionAccess<contract::Root>
    {
        static void* resolve(void* storage, std::uint64_t node, const WioNativeAbiContainerProjection& projection,
                             const WioNativeAbiContainerStep* steps, std::uint64_t count,
                             const std::type_info& requested)
        {
            auto* root = static_cast<contract::Root*>(storage);
            if (!root || node != contract::RootId || count != 1 || !steps ||
                steps->kind != WIO_NATIVE_ABI_CONTAINER_INDEX || steps->containerTypeId != node ||
                steps->index >= root->size())
                return nullptr;
            return NativePodProjectionAccess<contract::Pod>::resolve(&(*root)[steps->index], projection.podTypeId,
                                                                     projection.typeId, projection.offset,
                                                                     projection.size, requested);
        }
    };
} // namespace wio::wir_backend::abi
namespace
{
    namespace abi = wio::wir_backend::abi;
    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Wire
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    WioNativeAbiValue reference(void* data, std::uint64_t size, std::uint32_t flags = 0, std::uint64_t type = 0)
    {
        WioNativeAbiValue wire;
        wire.kind = WIO_NATIVE_ABI_REFERENCE;
        wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE | flags;
        wire.typeId = type;
        wire.payload.mutableSlice = {data, size};
        return wire;
    }
    void projections()
    {
        Wire root{abi::Codec<contract::Root, contract::RootId>::write({contract::Pod{}})};
        const auto rootRef = reference(&root.value, sizeof(root.value));
        const std::uint64_t index = 0;
        WioNativeAbiSequencePodProjection sequence{{&root.value, contract::RootId, contract::FieldId, &index, 1},
                                                   contract::PodId,
                                                   offsetof(contract::Pod, field),
                                                   sizeof(std::int32_t)};
        const auto sequenceRef =
            reference(&sequence, sizeof(sequence), WIO_NATIVE_ABI_VALUE_SEQUENCE_POD_PROJECTED, contract::FieldId);
        const WioNativeAbiContainerStep step{WIO_NATIVE_ABI_CONTAINER_INDEX, 0, contract::RootId, 0, {}};
        WioNativeAbiContainerProjection mixed{&root.value,
                                              contract::RootId,
                                              contract::FieldId,
                                              &step,
                                              1,
                                              contract::PodId,
                                              offsetof(contract::Pod, field),
                                              sizeof(std::int32_t)};
        const auto mixedRef =
            reference(&mixed, sizeof(mixed), WIO_NATIVE_ABI_VALUE_CONTAINER_PROJECTED, contract::FieldId);
        abi::Frame frame;
        auto& values = frame.borrow<contract::Root, contract::RootId>(rootRef, false);
        auto& first = frame.borrow<std::int32_t, contract::FieldId>(sequenceRef, true);
        auto& second = frame.borrow<std::int32_t, contract::FieldId>(mixedRef, true);
        check(&first == &second && &second == &values[0].field, "projection representation split alias identity");
        first = 31;
        check(second == 31 && frame.aliasesLiveBorrow<std::int32_t, contract::FieldId>(mixedRef, &first),
              "canonical borrow result did not map to its live cell");
        frame.commit();
        const auto committedOwner = root.value.payload.handle.state;
        frame.commit();
        check(committedOwner == root.value.payload.handle.state, "repeated commit allocated another root claim");
        check(abi::Codec<contract::Root, contract::RootId>::read(root.value)[0].field == 31,
              "mixed projection mutation lost copy-back");
        mixed.offset = offsetof(contract::Pod, hidden);
        bool rejected = false;
        try
        {
            (void)frame.borrow<std::int32_t, contract::FieldId>(mixedRef, true);
        }
        catch (const abi::Error&)
        {
            rejected = true;
        }
        check(rejected, "undeclared POD field bypassed registered layout");

        Wire pod{abi::Codec<contract::Pod, contract::PodId>::write({})};
        const auto podRef = reference(&pod.value, sizeof(pod.value));
        WioNativeAbiProjection field{&pod.value, contract::PodId, contract::FieldId, offsetof(contract::Pod, field),
                                     sizeof(std::int32_t)};
        const auto fieldRef = reference(&field, sizeof(field), WIO_NATIVE_ABI_VALUE_PROJECTED, contract::FieldId);
        abi::Frame podFrame;
        auto& value = podFrame.borrow<contract::Pod, contract::PodId>(podRef, false);
        check(&value.field == &podFrame.borrow<std::int32_t, contract::FieldId>(fieldRef, true),
              "POD projection failed to share its registered field cell");
        field.offset = offsetof(contract::Pod, hidden);
        rejected = false;
        try
        {
            (void)podFrame.borrow<std::int32_t, contract::FieldId>(fieldRef, true);
        }
        catch (const abi::Error&)
        {
            rejected = true;
        }
        check(rejected, "direct POD projection accepted undeclared storage");
    }
    struct Owner
    {
        int references = 1;
        int retains = 0;
        int releases = 0;
    };
    const WioNativeAbiHandleOps operations{WIO_NATIVE_ABI_VERSION, 0,
                                           [](void* state) noexcept
                                           {
                                               auto& owner = *static_cast<Owner*>(state);
                                               ++owner.references;
                                               ++owner.retains;
                                           },
                                           [](void* state) noexcept
                                           {
                                               auto& owner = *static_cast<Owner*>(state);
                                               --owner.references;
                                               ++owner.releases;
                                           },
                                           [](const void*) noexcept { return 901ULL; }};
    void ownership()
    {
        Owner owner;
        wio::bytecode::Module module;
        module.types.resize(1);
        module.types[0].kind = 28;
        module.types[0].nominalKind = 2;
        module.contract.reflection.push_back({.stableTypeId = 901, .type = 0});
        {
            const auto value = wio::vm::Value::foreignObject(0, 901, {&owner, &operations, 1});
            check(owner.references == 2, "VM did not retain its foreign object");
            Wire wire;
            std::string error;
            check(wio::vm::detail::encodeOwnedNativeValue(module, value, 0, 6, wire.value, error), error.c_str());
            check(owner.references == 3 && (wire.value.flags & WIO_NATIVE_ABI_VALUE_OWNED) &&
                      !(wire.value.flags & WIO_NATIVE_ABI_VALUE_BORROWED),
                  "owned encoding borrowed a foreign claim");
        }
        check(owner.references == 1 && owner.retains == 2 && owner.releases == 2,
              "foreign promotion double release or leak");
        for (bool invalid : {false, true})
        {
            WioNativeAbiValue wire;
            wire.kind = WIO_NATIVE_ABI_OBJECT;
            wire.flags = WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_CONSUMED;
            wire.payload.handle = {&owner, &operations, 1};
            operations.retain(&owner);
            std::array<WioNativeAbiValue, 2> arguments{wire, {}};
            try
            {
                abi::ConsumedArguments consumed(arguments.data(), arguments.size(),
                                                invalid ? std::initializer_list<std::size_t>{0, 1}
                                                        : std::initializer_list<std::size_t>{0});
                throw std::runtime_error("native call failed");
            }
            catch (const std::exception&)
            {
            }
            check(owner.references == 1, "consumed claim was not retired on decode/call failure");
            // The caller clears a transferred wire; it must not release it again.
            arguments[0] = {};
        }
    }
    void callbackAliases()
    {
        contract::Root root{{7, 19}};
        std::int32_t independent = 11;
        wio::wir_backend::CallbackBorrowCells<3> compatible;
        compatible.addCell(root);
        compatible.addCell(root);
        compatible.addCell(independent);
        for (bool reverse : {false, true})
        {
            bool rejected = false;
            try
            {
                wio::wir_backend::CallbackBorrowCells<2> cells;
                if (reverse)
                {
                    cells.addCell(root[0].field);
                    cells.addCell(root);
                }
                else
                {
                    cells.addCell(root);
                    cells.addCell(root[0].field);
                }
            }
            catch (const std::runtime_error& error)
            {
                rejected = std::string{error.what()}.find("overlapping callback") != std::string::npos;
            }
            check(rejected, "callback overlap silently split a live aggregate cell");
        }
    }
} // namespace
int main()
{
    try
    {
        projections();
        ownership();
        callbackAliases();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
