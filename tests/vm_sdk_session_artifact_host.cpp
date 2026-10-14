#include <wio_native_sdk.h>

#include <iostream>
#include <thread>

extern "C" const WioSdkModuleContract* WioStaticSessionContract() noexcept;
extern "C" const WioSdkModuleSessionApi* WioStaticSessionContractSessionApi() noexcept;

namespace
{
    using Host = wio::sdk::NativeModule;
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    template <class F> void rejects(F action)
    {
        bool failed = false;
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            failed = true;
        }
        expect(failed, "invalid session operation was accepted");
    }
    void qualify(Host first, Host second)
    {
        expect(first.contract() && second.contract() && first.contract() != second.contract() &&
                   first.contract()->stableId == second.contract()->stableId,
               "module opens do not have independent instances/stable identities");
        rejects([&] { first.invoke("Read"); });
        expect(first.apiVersion() == 11, "session API version failed");
        first.start();
        second.start();
        expect(first.invoke("Bump").asInteger() == 11 && second.invoke("Read").asInteger() == 10,
               "VM globals leak between sessions");
        first.start();
        expect(first.invoke("Read").asInteger() == 11, "idempotent start reinitialized globals");
        first.update(0.5);
        expect(first.invoke("Read").asInteger() == 16, "session update did not execute Wio body");
        auto snapshot = first.saveState();
        second.restoreState(snapshot);
        expect(second.invoke("Bump").asInteger() == 17 && first.invoke("Read").asInteger() == 16,
               "restored state aliases another instance");
        auto escaped = first.invoke("Message");
        auto clone = escaped.clone();
        auto alias = first;
        first.stop();
        rejects([&] { alias.invoke("Read"); });
        alias.close();
        first.close();
        expect(escaped.asString().starts_with("VM session closed safely") && escaped.asString() == clone.asString() &&
                   snapshot.asInteger() == 16,
               "closed session invalidated escaped SDK values");
        escaped.reset();
        std::thread release{[value = std::move(clone)]() mutable { value.reset(); }};
        release.join();
        std::thread close{[instance = std::move(second)]() mutable { instance.close(); }};
        close.join();
        expect(Host::collectSessions() == 1 && Host::collectSessions() == 0,
               "DLL/static instance did not retire exactly once on its owner thread");
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "shared module path required");
        const auto path = std::filesystem::u8path(argv[1]);
        qualify(Host::open(path), Host::open(path));
        const auto* api = WioStaticSessionContractSessionApi();
        expect(WioValidateSdkModuleSessionApi(api) && !WioStaticSessionContract(),
               "static lifecycle module did not expose session-only hosting");
        auto processLifetime = std::make_shared<int>(0);
        qualify(Host::attachSession(*api, processLifetime), Host::attachSession(*api, processLifetime));
        // Explicitly finish the VM, then leave only the returned wire as the
        // last DLL owner and release it from a foreign thread.
        auto last = Host::open(path);
        last.start();
        auto value = last.invoke("Message");
        last.close();
        std::thread finalRelease{[wire = std::move(value)]() mutable { wire.reset(); }};
        finalRelease.join();
        expect(Host::collectSessions() == 0, "ordinary result lifetime retained a closed VM session");
        auto reopened = Host::open(path);
        reopened.start();
        expect(reopened.invoke("Read").asInteger() == 10, "reloaded DLL retained a previous session");
        reopened.stop();
        std::cout << "vm-sdk-session-artifacts-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
