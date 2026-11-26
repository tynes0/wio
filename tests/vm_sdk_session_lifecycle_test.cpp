#include <wio/vm/module_session.h>
#include <wio_native_sdk.h>

#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <thread>

namespace
{
    namespace bc = wio::bytecode;
    using Factory = wio::vm::ModuleSessionFactory;
    using Host = wio::sdk::NativeModule;
    using Wire = wio::sdk::NativeValue;

    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class F> void rejects(F action)
    {
        bool rejected = false;
        try
        {
            action();
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        expect(rejected, "invalid lifecycle operation was accepted");
    }

    bc::Module fixture()
    {
        bc::Module module;
        bc::StringTableBuilder strings{module};
        module.name = module.logicalName = strings.intern("session-test");
        module.stableKey = strings.intern("wio.module:session-test");
        module.stableId = WioStableSdkId(module.strings[module.stableKey].c_str());
        module.moduleKind = WIO_SDK_MODULE_WIO_LIBRARY;
        module.abiDescriptorVersion = 11;
        module.contract.callTableStableId = 1;
        const auto empty = strings.intern("");
        for (auto kind : {5, 10, 13, 1, 17})
            module.types.push_back(bc::Type{.kind = static_cast<std::uint8_t>(kind), .name = empty});
        module.constants = {
            bc::Constant{}, bc::Constant{.kind = bc::ConstantKind::SignedInteger, .bits = 0},
            bc::Constant{.kind = bc::ConstantKind::UnsignedInteger, .bits = 11},
            bc::Constant{.kind = bc::ConstantKind::String, .string = strings.intern(std::string{"A\0B", 3})}};
        const auto function =
            [&](const char* name, std::uint32_t result, std::vector<std::uint32_t> parameters, std::uint32_t constant)
        {
            bc::Function fn;
            fn.id = static_cast<std::uint32_t>(module.functions.size());
            fn.name = strings.intern(name);
            fn.returnType = result;
            for (auto type : parameters)
                fn.parameters.push_back(bc::Parameter{
                    .value = static_cast<std::uint32_t>(fn.parameters.size()), .name = empty, .type = type});
            bc::Block block;
            block.id = 0;
            if (result != 3 && constant != bc::InvalidIndex)
                block.instructions.push_back(bc::Instruction{.opcode = bc::Opcode::Constant,
                                                             .result = static_cast<std::uint32_t>(parameters.size()),
                                                             .resultType = result,
                                                             .constant = constant});
            bc::Instruction ret{.opcode = bc::Opcode::Return, .constant = 0};
            if (result != 3)
                ret.operands = {constant == bc::InvalidIndex ? 0u : static_cast<std::uint32_t>(parameters.size())};
            block.instructions.push_back(std::move(ret));
            fn.blocks.push_back(std::move(block));
            module.functions.push_back(std::move(fn));
        };
        function("Identity", 0, {0}, bc::InvalidIndex);
        function("Version", 1, {}, 2);
        function("Load", 0, {}, 1);
        function("Update", 3, {2}, 0);
        function("Unload", 3, {}, 0);
        function("Save", 4, {}, 3);
        function("Restore", 0, {4}, 1);
        bc::Export entry;
        entry.stableKey = strings.intern("function:Identity");
        entry.stableId = WioStableSdkId(module.strings[entry.stableKey].c_str());
        entry.logicalName = entry.symbolName = module.functions[0].name;
        entry.roleName = empty;
        entry.function = 0;
        entry.parameterTypes = {0};
        entry.returnType = 0;
        module.contract.exports.push_back(entry);
        module.contract.callTableEntries = {entry.stableId};
        module.contract.lifecycle = {.apiVersion = 1,
                                     .load = 2,
                                     .update = 3,
                                     .unload = 4,
                                     .saveState = 5,
                                     .restoreState = 6,
                                     .stateSchemaVersion = 7};
        return module;
    }

    class Observer final : public wio::vm::DebugObserver
    {
    public:
        std::array<unsigned, 7> calls{};
        std::function<void()> callback;
        wio::vm::DebugAction onInstruction(const wio::vm::DebugEvent& event) override
        {
            if (callback)
            {
                auto action = std::move(callback);
                action();
            }
            if (event.instruction == 0 && event.function < calls.size())
                ++calls[event.function];
            return wio::vm::DebugAction::Continue;
        }
    };
} // namespace

int main()
{
    try
    {
        Observer observer;
        auto factory = std::make_shared<Factory>(fixture(), wio::vm::MachineOptions{.debugObserver = &observer});
        expect(WioValidateSdkModuleSessionApi(&factory->api()), "invalid session factory ABI");
        rejects([&] { (void)factory->defaultContract(); });
        auto one = Host::attachSession(factory->api(), factory);
        auto two = Host::attachSession(factory->api(), factory);
        expect(one.contract() != two.contract() && one.contract()->stateSchemaVersion == 7,
               "sessions share descriptor/context storage");
        std::array args{Wire::integer(42)};
        rejects([&] { one.invoke("Identity", args); });
        rejects([&] { one.update(0.1); });
        expect(one.apiVersion() == 11, "version query before start failed");
        one.start();
        one.start();
        expect(observer.calls[2] == 1, "start called Load twice");
        bool wrongStart = false;
        std::thread starter{[view = one, &wrongStart]() mutable
                            {
                                try
                                {
                                    view.start();
                                }
                                catch (const std::runtime_error&)
                                {
                                    wrongStart = true;
                                }
                            }};
        starter.join();
        expect(wrongStart, "idempotent SDK start bypassed thread affinity");
        expect(one.invoke("Identity", args).asInteger() == 42, "started session export failed");
        rejects([&] { one.update(-1); });
        rejects([&] { one.update((std::numeric_limits<double>::infinity)()); });
        rejects([&] { one.update((std::numeric_limits<double>::quiet_NaN)()); });
        one.update(0.5);
        expect(observer.calls[3] == 1, "invalid delta executed user update");
        auto snapshot = one.saveState();
        expect(snapshot.asString() == std::string{"A\0B", 3}, "saved state is not byte-exact");
        rejects([&] { one.restoreState(Wire::boolean(true)); });
        one.restoreState(snapshot);
        expect(observer.calls[6] == 1, "state restore type validation failed");
        one.stop();
        one.stop();
        one.close();
        expect(observer.calls[4] == 1 && snapshot.asString() == std::string{"A\0B", 3},
               "unload was repeated or destroyed the snapshot");
        rejects([&] { one.invoke("Identity", args); });
        two.start();
        std::thread foreign{[instance = std::move(two)]() mutable { instance.close(); }};
        foreign.join();
        expect(observer.calls[4] == 1, "foreign-thread destructor ran affine user unload");
        expect(Host::collectSessions() == 1 && observer.calls[4] == 2,
               "owner safe point did not reclaim retired session");
        expect(Host::collectSessions() == 0, "session retirement repeated cleanup");

        WioSdkModuleSession raw{};
        const auto& api = factory->api();
        expect(api.open(api.factory, &raw) == WIO_NATIVE_ABI_OK, "raw session open failed");
        expect(api.open(api.factory, &raw) == WIO_NATIVE_ABI_INVALID_ARGUMENT, "open overwrote an existing session");
        std::int32_t status = 0;
        std::thread closer{[&] { status = api.close(&raw); }};
        closer.join();
        expect(status == WIO_NATIVE_ABI_WRONG_THREAD && raw.context && raw.contract,
               "wrong-thread close destroyed raw state");
        expect(api.close(&raw) == WIO_NATIVE_ABI_OK && !raw.context && !raw.contract,
               "raw session close did not clear its token");
        expect(api.close(&raw) == WIO_NATIVE_ABI_OK && api.start(&raw) == WIO_NATIVE_ABI_STALE_HANDLE,
               "empty session behavior is not idempotent/stale");
        std::weak_ptr<Factory> weak = factory;
        factory.reset();
        expect(!weak.expired(), "snapshot did not pin producer/factory lifetime");
        std::thread resultRelease{[value = std::move(snapshot)]() mutable { value.reset(); }};
        resultRelease.join();
        expect(weak.expired(), "last result retained a closed VM or factory");

        Observer rollback;
        auto bad = fixture();
        bad.constants.push_back(bc::Constant{.kind = bc::ConstantKind::SignedInteger, .bits = 5});
        bad.functions[2].blocks[0].instructions[0].constant = 4;
        auto failedFactory =
            std::make_shared<Factory>(std::move(bad), wio::vm::MachineOptions{.debugObserver = &rollback});
        auto failed = Host::attachSession(failedFactory->api(), failedFactory);
        rejects([&] { failed.start(); });
        rejects([&] { failed.start(); });
        expect(rollback.calls[2] == 1 && rollback.calls[4] == 1, "failed start did not rollback exactly once");
        failed.close();
        expect(rollback.calls[4] == 1, "failed-start destructor ran unload twice");

        Observer unloadFailure;
        auto broken = fixture();
        broken.functions[4].blocks[0].instructions = {
            bc::Instruction{.opcode = bc::Opcode::Unreachable, .constant = 0}};
        auto brokenFactory =
            std::make_shared<Factory>(std::move(broken), wio::vm::MachineOptions{.debugObserver = &unloadFailure});
        auto brokenHost = Host::attachSession(brokenFactory->api(), brokenFactory);
        brokenHost.start();
        rejects([&] { brokenHost.stop(); });
        expect(!brokenHost.contract() && unloadFailure.calls[4] == 1, "failed unload did not destroy the session");
        brokenHost.close();
        expect(unloadFailure.calls[4] == 1, "failed unload retried during destruction");

        Observer reentry;
        auto reentryFactory = std::make_shared<Factory>(fixture(), wio::vm::MachineOptions{.debugObserver = &reentry});
        auto duringStart = Host::attachSession(reentryFactory->api(), reentryFactory);
        reentry.callback = [&] { duringStart.close(); };
        duringStart.start();
        expect(!duringStart.contract() && reentry.calls[2] == 1 && reentry.calls[4] == 1,
               "closing a view during start destroyed an active session or skipped unload");
        auto duringCall = Host::attachSession(reentryFactory->api(), reentryFactory);
        duringCall.start();
        reentry.callback = [&] { duringCall.close(); };
        auto answer = duringCall.invoke("Identity", args);
        expect(answer.asInteger() == 42 && !duringCall.contract() && reentry.calls[4] == 2,
               "closing a view during invocation invalidated the active call");

        WioSdkModuleSession guarded{};
        const auto& guardedApi = reentryFactory->api();
        expect(guardedApi.open(guardedApi.factory, &guarded) == WIO_NATIVE_ABI_OK, "guarded session open failed");
        std::int32_t nestedClose = 0;
        reentry.callback = [&] { nestedClose = guardedApi.close(&guarded); };
        expect(guardedApi.start(&guarded) == WIO_NATIVE_ABI_OK && nestedClose == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                   guarded.context && guarded.contract,
               "raw reentrant close destroyed an active instance");
        expect(guardedApi.close(&guarded) == WIO_NATIVE_ABI_OK, "guarded session did not close at safe point");
        rejects(
            []
            {
                auto m = fixture();
                m.contract.lifecycle.restoreState = bc::InvalidIndex;
                Factory f{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.functions[5].returnType = 3;
                m.functions[5].blocks[0].instructions = {bc::Instruction{.opcode = bc::Opcode::Return, .constant = 0}};
                Factory f{std::move(m)};
            });
        std::cout << "vm-sdk-session-lifecycle-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
