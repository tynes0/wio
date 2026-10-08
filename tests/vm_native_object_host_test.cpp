#include "native_object_internal.h"
#include "native_bridge_internal.h"
#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wir_object_abi.h>

#include <array>
#include <atomic>
#include <iostream>
#include <thread>

namespace
{
    namespace abi = wio::wir_backend::abi;
    namespace bridge = wio::vm::detail;
    namespace bc = wio::bytecode;
    using wio::vm::Value;
    constexpr std::uint64_t ObjectId = 100;
    constexpr std::uint64_t ReadId = 501;
    constexpr std::uint64_t AddId = 502;

    bool expect(bool value, const char* message)
    {
        if (!value)
            std::cerr << message << '\n';
        return value;
    }

    bc::Module module()
    {
        bc::Module output;
        output.abiDescriptorVersion = 1;
        output.contract.callTableStableId = 1;
        output.constants = {{}};
        output.types = {
            {.kind = 28, .nominalKind = 2, .ownership = 2, .cleanup = 2, .fields = {{.type = 1}}, .castTypes = {0}},
            {.kind = 5},
            {.kind = 29, .arguments = {0}, .flags = 1},
            {.kind = 29, .arguments = {1}, .flags = 1},
        };
        output.contract.reflection = {{.stableTypeId = ObjectId, .type = 0, .nominalKind = 2},
                                      {.stableTypeId = 11, .type = 1}};
        bc::Instruction field{.opcode = bc::Opcode::FieldPlace,
                              .result = 1,
                              .resultType = 3,
                              .operands = {0},
                              .projectionIndex = 0,
                              .targetType = 0};
        bc::Instruction load{.opcode = bc::Opcode::Load, .result = 2, .resultType = 1, .operands = {1}};
        bc::Instruction ret{.opcode = bc::Opcode::Return, .operands = {2}};
        output.functions = {{.id = 0,
                             .parameters = {{.value = 0, .type = 2}},
                             .returnType = 1,
                             .blocks = {{.id = 0, .instructions = {field, load, ret}}}}};
        field.result = 2;
        load.result = 3;
        load.operands = {2};
        bc::Instruction add{.opcode = bc::Opcode::Binary, .result = 4, .resultType = 1, .operands = {3, 1}};
        bc::Instruction store{.opcode = bc::Opcode::Store, .operands = {2, 4}};
        ret.operands = {4};
        output.functions.push_back({.id = 1,
                                    .parameters = {{.value = 0, .type = 2}, {.value = 1, .type = 1}},
                                    .returnType = 1,
                                    .blocks = {{.id = 0, .instructions = {field, load, add, store, ret}}}});
        // The canonical bytecode envelope assigns the null constant slot even
        // to instructions that do not consume a literal.
        for (auto& function : output.functions)
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    instruction.constant = 0;
        return output;
    }

    int hostDestroyed = 0;
    int proxyDestroyed = 0;

    struct Host
    {
        std::atomic_uint32_t references{1};
        std::shared_ptr<wio::vm::Machine> machine;
        Value object;
        bc::Module program;
        ~Host()
        {
            ++hostDestroyed;
        }
    };

    void retain(void* state) noexcept
    {
        ++static_cast<Host*>(state)->references;
    }
    void release(void* state) noexcept
    {
        auto* host = static_cast<Host*>(state);
        if (--host->references == 0)
            delete host;
    }
    std::uint64_t typeId(const void*) noexcept
    {
        return ObjectId;
    }
    const WioNativeAbiHandleOps hostOps{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};

    int resultOwners = 0;
    void retainResult(void*) noexcept
    {
        ++resultOwners;
    }
    void releaseResult(void*) noexcept
    {
        --resultOwners;
    }
    const WioNativeAbiHandleOps resultOps{WIO_NATIVE_ABI_VERSION, 0, &retainResult, &releaseResult, &typeId};

    WioNativeAbiStatus invoke(const WioNativeObjectHost* token, std::uint64_t method,
                              const WioNativeAbiValue* arguments, std::uint64_t count, WioNativeAbiValue* result,
                              WioNativeAbiFailure* failure) noexcept
    {
        return abi::ContainerAdapter<Value, 0>::boundary(
            failure,
            [&]
            {
                abi::require(token && token->owner.state && result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                auto& host = *static_cast<Host*>(token->owner.state);
                if (token->generation != 17 || token->objectId != 301)
                    throw abi::Error(WIO_NATIVE_ABI_STALE_HANDLE);
                if (method == 503)
                {
                    ++resultOwners;
                    result->kind = WIO_NATIVE_ABI_BYTES;
                    result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
                    result->owner = {&resultOwners, &resultOps, 1};
                    throw std::runtime_error("host-dispatch-failure");
                }
                if (method == 504)
                {
                    abi::require(count == 0, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                    *result = {};
                    return;
                }
                abi::require((method == ReadId && count == 0) || (method == AddId && count == 1),
                             WIO_NATIVE_ABI_INVALID_ARGUMENT);
                std::vector<Value> values{host.object};
                if (method == AddId)
                {
                    abi::require(arguments, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                    auto decoded = bridge::decodeNativeValue(host.program, arguments[0], 1, 1);
                    abi::require(decoded.succeeded());
                    values.push_back(std::move(decoded.value));
                }
                auto execution = host.machine->invoke(method == ReadId ? 0 : 1, values);
                if (!execution.succeeded())
                {
                    std::cerr << execution.error().code << ": " << execution.error().message << '\n';
                    throw abi::Error(WIO_NATIVE_ABI_PANIC);
                }
                std::string error;
                abi::require(bridge::encodeOwnedNativeValue(host.program, execution.value(), 1, 1, *result, error));
            });
    }

    struct Counter : wio::runtime::RefCountedObject
    {
        virtual std::int32_t Read() const = 0;
        virtual std::int32_t Add(std::int32_t value) = 0;
        ~Counter() override
        {
            ++proxyDestroyed;
        }
    };

    struct Proxy final : abi::HostObject<Counter>
    {
        explicit Proxy(const WioNativeObjectHost& host) : HostObject(host)
        {
        }
        std::int32_t dispatch(std::uint64_t method, const WioNativeAbiValue* args, std::uint64_t count) const
        {
            return hostLease().call<std::int32_t, 11>(method, args, count);
        }
        std::int32_t Read() const override
        {
            return dispatch(ReadId, nullptr, 0);
        }
        std::int32_t Add(std::int32_t value) override
        {
            const auto argument = abi::Codec<std::int32_t, 11>::write(value);
            return dispatch(AddId, &argument, 1);
        }
    };

    using Adapter = abi::ObjectAdapter<Counter, Proxy, ObjectId>;
    struct ThrowingProxy final : abi::HostObject<Counter>
    {
        explicit ThrowingProxy(const WioNativeObjectHost& host) : HostObject(host)
        {
            throw std::runtime_error("proxy-construction-failure");
        }
        std::int32_t Read() const override
        {
            return 0;
        }
        std::int32_t Add(std::int32_t) override
        {
            return 0;
        }
    };
    using ThrowingAdapter = abi::ObjectAdapter<Counter, ThrowingProxy, ObjectId>;
    using Ref = wio::runtime::Ref<Counter>;
    WioNativeObjectDescriptor descriptor()
    {
        return {ObjectId,         WIO_NATIVE_OBJECT_HOST_PROXY | WIO_NATIVE_OBJECT_WEAK_CACHE,
                &Adapter::create, &Adapter::host,
                &Adapter::weak,   &Adapter::lock};
    }
} // namespace

int qualify()
{
    auto source = module();
    auto decoded = bc::decode(bc::encode(source));
    bool passed = expect(decoded.succeeded(), "object host fixture failed the real bytecode codec");
    if (!passed)
        return 1;
    auto machine = std::make_shared<wio::vm::Machine>(decoded.module);
    auto* host = new Host{.machine = machine, .object = Value::object(0, {Value::signedInteger(7)}), .program = source};
    WioNativeObjectHost token{
        .typeId = ObjectId, .objectId = 301, .generation = 17, .owner = {host, &hostOps, 17}, .invoke = &invoke};
    auto adapter = descriptor();
    const WioNativeObjectApi api{1, 1, &adapter};
    bridge::NativePodLayouts layouts;
    std::string error;
    passed &= expect(bridge::mergeNativeObjects(source, api, layouts, error), "object descriptor registration failed");
    const WioNativeAbiRegistry empty{WIO_NATIVE_ABI_VERSION, 0, nullptr};
    passed &= expect(machine->registerNativeRegistry(empty, api, &error), "machine object registration failed");

    WioNativeAbiValue wire{};
    WioNativeAbiFailure failure{};
    auto invalidToken = token;
    invalidToken.generation += 1;
    passed &= expect(Adapter::create(&invalidToken, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                         wire.kind == WIO_NATIVE_ABI_VOID && host->references == 1 && proxyDestroyed == 0,
                     "mismatched host generation entered the proxy constructor");
    passed &= expect(ThrowingAdapter::create(&token, &wire, &failure) == WIO_NATIVE_ABI_EXCEPTION &&
                         wire.kind == WIO_NATIVE_ABI_VOID && host->references == 1 && proxyDestroyed == 1,
                     "failed proxy construction leaked its acquired host claim");
    // The throwing constructor destroyed only its partial native base.
    proxyDestroyed = 0;
    passed &=
        expect(Adapter::create(&token, &wire, &failure) == WIO_NATIVE_ABI_OK, "typed object proxy creation failed");
    passed &= expect(host->references == 2, "proxy did not retain exactly one host claim");
    {
        abi::ObjectHostLease lease(token);
        WioNativeAbiValue result{};
        passed &= expect(lease.invoke(0, nullptr, 0, &result, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                             lease.invoke(ReadId, nullptr, 1, &result, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                             lease.invoke(ReadId, nullptr, 0, nullptr, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                         "invalid method/output/argument buffer entered host dispatch");
        lease.reset();
        lease.reset();
        passed &= expect(host->references == 2 &&
                             lease.invoke(ReadId, nullptr, 0, &result, &failure) == WIO_NATIVE_ABI_STALE_HANDLE,
                         "lease reset was not idempotent or dispatch reused its released claim");
    }
    WioNativeObjectHost recovered{};
    passed &= expect(Adapter::host(&wire, &recovered, &failure) == WIO_NATIVE_ABI_OK && recovered.objectId == 301 &&
                         recovered.generation == 17 && recovered.owner.state == host && host->references == 2,
                     "object identity recovery copied or retained the host");
    WioNativeAbiHandle weak{};
    passed &= expect(Adapter::weak(&wire, &weak, &failure) == WIO_NATIVE_ABI_OK && host->references == 2,
                     "weak proxy cache retained the VM object");
    WioNativeAbiValue locked{};
    passed &= expect(Adapter::lock(&weak, &locked, &failure) == WIO_NATIVE_ABI_OK &&
                         locked.payload.handle.state == wire.payload.handle.state,
                     "weak lock changed object identity");
    {
        auto reference = abi::Codec<Ref, ObjectId>::read(wire);
        passed &=
            expect(reference->Read() == 7 && reference->Add(11) == 18 && host->object.field(0)->asSignedInteger() == 18,
                   "native virtual calls did not mutate the original VM object");
        auto copy = reference;
        passed &= expect(copy->Read() == 18 && host->references == 2, "native Ref copy cloned the hosted VM object");
        static_cast<const Proxy*>(reference.Get())->hostLease().call<void, 0>(504, nullptr, 0);
        try
        {
            static_cast<const Proxy*>(reference.Get())->dispatch(503, nullptr, 0);
            passed &= expect(false, "host failure was silently ignored");
        }
        catch (const abi::ObjectDispatchError& error)
        {
            passed &= expect(error.status == WIO_NATIVE_ABI_EXCEPTION &&
                                 std::string_view(error.what()) == "host-dispatch-failure" && resultOwners == 0,
                             "host failure lost its diagnostic or leaked a partially owned result");
        }
        std::atomic<WioNativeAbiStatus> foreign{WIO_NATIVE_ABI_OK};
        std::thread worker(
            [&]
            {
                WioNativeAbiValue result{};
                WioNativeAbiFailure failure{};
                foreign = static_cast<const Proxy*>(reference.Get())
                              ->hostLease()
                              .invoke(ReadId, nullptr, 0, &result, &failure);
                WioNativeAbiReleaseValue(&result);
            });
        worker.join();
        passed &= expect(foreign == WIO_NATIVE_ABI_WRONG_THREAD, "hosted method entered the VM from a foreign thread");
    }
    // Drop the caller's claim: native references now own the sole VM host claim.
    release(host);
    machine.reset();
    {
        auto reference = abi::Codec<Ref, ObjectId>::read(locked);
        passed &= expect(reference->Read() == 18 && hostDestroyed == 0, "native-held object outlived its VM host");
    }
    WioNativeAbiReleaseValue(&wire);
    passed &= expect(hostDestroyed == 0, "an aliased native reference released its host too early");
    WioNativeAbiReleaseValue(&locked);
    passed &= expect(hostDestroyed == 1 && proxyDestroyed == 0,
                     "final strong release did not release host independently of weak memory lifetime");
    passed &= expect(Adapter::lock(&weak, &locked, &failure) == WIO_NATIVE_ABI_STALE_HANDLE &&
                         locked.kind == WIO_NATIVE_ABI_VOID,
                     "expired weak cache resurrected a VM object");
    WioNativeAbiRelease(&weak);
    passed &= expect(proxyDestroyed == 1 && hostDestroyed == 1, "host/proxy final cleanup was not exactly once");

    auto invalid = api;
    invalid.version = 99;
    bridge::NativePodLayouts unpublished;
    passed &= expect(!bridge::mergeNativeObjects(source, invalid, unpublished, error) && unpublished.objects.empty(),
                     "invalid object sidecar version was published");
    adapter.flags = WIO_NATIVE_OBJECT_HOST_PROXY;
    passed &= expect(!bridge::mergeNativeObjects(source, api, unpublished, error),
                     "object adapter without weak caching accepted");
    adapter = descriptor();
    passed &= expect(bridge::mergeNativeObjects(source, api, layouts, error),
                     "identical object registration was not idempotent");
    adapter.create = &ThrowingAdapter::create;
    passed &= expect(!bridge::mergeNativeObjects(source, api, layouts, error) &&
                         layouts.objects.at(0).create == &Adapter::create,
                     "conflicting producer replaced the registered object adapter");
    adapter = descriptor();
    adapter.typeId = 11;
    passed &=
        expect(!bridge::mergeNativeObjects(source, api, unpublished, error), "scalar was registered as an object");
    adapter = descriptor();
    adapter.host = nullptr;
    passed &= expect(!bridge::mergeNativeObjects(source, api, unpublished, error),
                     "object without identity recovery accepted");
    adapter = descriptor();
    std::array batch{adapter, adapter};
    batch[1].lock = nullptr;
    const WioNativeObjectApi partial{1, 2, batch.data()};
    passed &= expect(!bridge::mergeNativeObjects(source, partial, unpublished, error) && unpublished.objects.empty(),
                     "partial object registration escaped its transaction");
    token.owner = {};
    passed &= expect(Adapter::create(&token, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                         wire.kind == WIO_NATIVE_ABI_VOID && proxyDestroyed == 1,
                     "invalid owner ran the proxy constructor or escaped a result");
    passed &=
        expect(Adapter::host(nullptr, &recovered, &failure) != WIO_NATIVE_ABI_OK, "null object wire was accepted");
    passed &= expect(Adapter::lock(&weak, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "released weak storage was accepted");
    return passed ? 0 : 1;
}

int main()
{
    try
    {
        return qualify();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Object host qualification threw: " << error.what() << '\n';
        return 1;
    }
}
