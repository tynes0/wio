#include "native_pod_internal.h"
#include "wio/vm/machine.h"
#include "wir_abi.h"

#include <iostream>

namespace
{
    namespace bc = wio::bytecode;
    namespace bridge = wio::vm::detail;
    constexpr std::uint64_t PodId = 801;
    int constructions = 0;
    int releases = 0;

    int next()
    {
        ++constructions;
        return 17;
    }

    struct Pod
    {
        std::int32_t value = next();
    };

    const WioNativeAbiHandleOps Operations{WIO_NATIVE_ABI_VERSION, 0, [](void*) noexcept {},
                                           [](void* value) noexcept
                                           {
                                               ++releases;
                                               delete static_cast<Pod*>(value);
                                           },
                                           [](const void*) noexcept { return PodId; }};

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    WioNativeAbiStatus create(WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        auto* value = new Pod{};
        *result = {};
        result->kind = WIO_NATIVE_ABI_POD;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = PodId;
        result->payload.slice = {value, sizeof(Pod)};
        result->owner = {value, &Operations, 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus failing(WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        (void)create(result, failure);
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus malformed(WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        (void)create(result, failure);
        result->typeId += 1;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus badSize(WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        (void)create(result, failure);
        --result->payload.slice.size;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus conflictingFlags(WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        (void)create(result, failure);
        result->flags |= WIO_NATIVE_ABI_VALUE_BORROWED;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus pack(const WioNativeAbiValue*, const WioNativeAbiValue* fields, std::uint64_t count,
                            WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        if (!fields || count != 1)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        (void)create(result, failure);
        static_cast<Pod*>(result->owner.state)->value = static_cast<std::int32_t>(fields[0].payload.signedInteger);
        return WIO_NATIVE_ABI_OK;
    }

    bool failRead = false;
    WioNativeAbiStatus read(const WioNativeAbiValue* source, std::uint64_t index, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (index != 0 || failRead)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        *result = {};
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = static_cast<const Pod*>(source->payload.slice.data)->value;
        return WIO_NATIVE_ABI_OK;
    }

    bc::Module makeModule(bool memberwise = false)
    {
        bc::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"value", "Pod", "Entry"};
        module.constants = {bc::Constant{.kind = bc::ConstantKind::SignedInteger, .bits = 29}};
        module.types = {bc::Type{.kind = 5}, bc::Type{.kind = 28,
                                                      .name = 1,
                                                      .nominalKind = 1,
                                                      .nominalRepresentation = 1,
                                                      .fields = {{.name = 0, .type = 0}}}};
        module.types[1].hasNativeBinding = true;
        module.contract.reflection = {{.stableTypeId = PodId, .logicalName = 1, .type = 1, .nominalKind = 1}};
        bc::Instruction constant;
        constant.opcode = bc::Opcode::Constant;
        constant.result = 0;
        constant.resultType = 0;
        constant.constant = 0;
        bc::Instruction construct;
        construct.opcode = bc::Opcode::ConstructComponent;
        construct.result = 1;
        construct.resultType = 1;
        construct.constant = 0;
        if (memberwise)
            construct.operands = {0};
        bc::Instruction finish;
        finish.opcode = bc::Opcode::Return;
        finish.constant = 0;
        finish.operands = {1};
        bc::Function entry;
        entry.id = 0;
        entry.name = 2;
        entry.returnType = 1;
        entry.blocks = {bc::Block{.id = 0, .instructions = {constant, construct, finish}}};
        module.functions.push_back(std::move(entry));
        return module;
    }
} // namespace

int main()
{
    const auto module = makeModule();
    const WioNativePodFieldDescriptor field{"value", 802, 0, sizeof(Pod), 5};
    const WioNativePodLayoutDescriptor layout{PodId, sizeof(Pod), alignof(Pod), 1, &field, &pack, &read};
    const WioNativePodLayoutApi layoutApi{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &layout};
    WioNativePodDefaultDescriptor factory{PodId, &create};
    WioNativePodDefaultApi defaults{WIO_NATIVE_POD_DEFAULT_VERSION, 1, &factory};
    bridge::NativePodLayouts layouts;
    std::string error;
    bool passed = expect(bridge::mergeNativePodLayouts(module, layoutApi, layouts, error), error.c_str());
    passed &= expect(!bridge::defaultNativePod(module, 1, layouts).succeeded(), "legacy producer invented defaults");
    passed &= expect(bridge::mergeNativePodDefaults(module, defaults, layouts, error), error.c_str());
    passed &= expect(bridge::mergeNativePodDefaults(module, defaults, layouts, error), "idempotent factory rejected");
    const auto previous = layouts.defaults;

    const auto rejected = [&]
    { return !bridge::mergeNativePodDefaults(module, defaults, layouts, error) && layouts.defaults == previous; };
    factory.create = &failing;
    passed &= expect(rejected(), "conflicting factory replaced its published snapshot");
    factory.create = nullptr;
    passed &= expect(rejected(), "null default factory accepted");
    factory.create = &create;
    factory.typeId = 0;
    passed &= expect(rejected(), "zero factory identity accepted");
    factory.typeId = PodId;
    ++defaults.version;
    passed &= expect(rejected(), "wrong default sidecar version accepted");
    --defaults.version;
    defaults.types = nullptr;
    passed &= expect(rejected(), "non-empty null default descriptor array accepted");
    defaults.types = &factory;
    bridge::NativePodLayouts absent;
    passed &=
        expect(!bridge::mergeNativePodDefaults(module, defaults, absent, error), "factory without layout accepted");

    const int before = releases;
    const auto value = bridge::defaultNativePod(module, 1, layouts);
    passed &= expect(value.succeeded() && value.value.field(0)->asSignedInteger() == 17 && releases == before + 1,
                     "default value did not copy producer bytes and release owned factory output");
    for (const auto adapter : {&failing, &malformed, &badSize, &conflictingFlags})
    {
        auto faulty = layouts;
        faulty.defaults[1] = adapter;
        const int releasedBefore = releases;
        passed &= expect(!bridge::defaultNativePod(module, 1, faulty).succeeded() && releases == releasedBefore + 1,
                         "invalid/failed default factory leaked partially produced owned output");
    }
    failRead = true;
    const int readBefore = releases;
    passed &= expect(!bridge::defaultNativePod(module, 1, layouts).succeeded() && releases == readBefore + 1,
                     "default field read failure leaked producer bytes");
    failRead = false;

    WioNativeAbiValue wire;
    (void)create(&wire, nullptr);
    const int countBefore = constructions;
    const auto decoded = wio::wir_backend::abi::Codec<Pod, PodId>::read(wire);
    passed &= expect(decoded.value == 17 && constructions == countBefore,
                     "POD byte decoding executed member default initializers");
    WioNativeAbiReleaseValue(&wire);

    const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 0, nullptr};
    wio::vm::Machine legacy{module};
    passed &= expect(legacy.registerNativeRegistry(registry, layoutApi, nullptr, nullptr, &error), error.c_str());
    const auto missing = legacy.invoke(0);
    passed &=
        expect(!missing.succeeded() && missing.error().code == "WVM1223", "missing factory silently produced zeros");
    const auto explicitModule = makeModule(true);
    wio::vm::Machine explicitLegacy{explicitModule};
    passed &=
        expect(explicitLegacy.registerNativeRegistry(registry, layoutApi, nullptr, nullptr, &error), error.c_str());
    const auto explicitValue = explicitLegacy.invoke(0);
    passed &= expect(explicitValue.succeeded() && explicitValue.value().field(0)->asSignedInteger() == 29,
                     "legacy fully specified memberwise construction regressed");

    wio::vm::Machine machine{module};
    passed &=
        expect(machine.registerNativeRegistry(registry, layoutApi, nullptr, nullptr, &error, &defaults), error.c_str());
    const int machineBefore = constructions;
    const auto result = machine.invoke(0);
    passed &= expect(result.succeeded() && result.value().field(0)->asSignedInteger() == 17 &&
                         constructions == machineBefore + 1,
                     "default-only VM module did not construct exactly once");
    return passed ? 0 : 1;
}
