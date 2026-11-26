#include <wio_native_sdk.h>

#include <array>
#include <iostream>
#include <thread>

extern "C" const WioSdkModuleSessionApi* WioStaticTaskInputsContractSessionApi() noexcept;
extern "C" const WioSdkModuleTaskApi* WioStaticTaskInputsContractTaskApi() noexcept;

namespace
{
    using Host = wio::sdk::NativeModule;
    using Value = wio::sdk::NativeValue;

    void expect(bool condition, const char* message)
    {
        if (!condition)
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
        expect(rejected, "invalid task input was accepted");
    }

    const WioSdkExportDescriptor& entry(const Host& module, std::string_view name)
    {
        const auto* contract = module.contract();
        for (std::uint32_t index = 0; index < contract->exportCount; ++index)
            if (contract->exports[index].logicalName == name)
                return contract->exports[index];
        throw std::runtime_error("task input export not found");
    }

    void rawReject(const Host& module, std::string_view name, std::span<const WioNativeAbiValue> args,
                   std::int32_t expected = WIO_NATIVE_ABI_TYPE_MISMATCH)
    {
        const auto& call = module.contract()->callTable[entry(module, name).callTableSlot];
        WioNativeAbiValue output{};
        const auto status = call.invoke(args.data(), static_cast<std::uint32_t>(args.size()), &output, call.context);
        const bool empty = output.kind == WIO_NATIVE_ABI_VOID && output.flags == 0 && output.typeId == 0 &&
                           !output.owner.state && !output.owner.ops;
        WioNativeAbiReleaseValue(&output);
        expect(status == expected && empty, "raw task input validation crossed into a body or published a result");
    }

    void qualify(Host module, Host other)
    {
        bool inputOnlyIdentity = false;
        for (std::uint32_t index = 0; index < module.contract()->reflectionCount; ++index)
        {
            const auto& reflected = module.contract()->reflection[index];
            if (std::string_view{reflected.logicalName} == "async-task::view<u32::view<>;>")
                inputOnlyIdentity = reflected.stableTypeId == WioStableSdkId(reflected.logicalName);
        }
        expect(inputOnlyIdentity, "task parameter-only structural ABI identity was not emitted");
        std::array numberArgs{Value::integer(41)};
        auto source = module.invoke("Number", numberArgs);
        expect(!module.taskReady(source), "input task completed before main pumping");
        std::array addArgs{source.clone(), Value::integer(1)};
        rejects([&] { other.invoke("Add", addArgs); });
        std::array badBorrow{source.borrow(false), Value::integer(1)};
        rejects([&] { module.invoke("Add", badBorrow); });
        std::array badScalar{Value::integer(41), Value::integer(1)};
        rejects([&] { module.invoke("Add", badScalar); });

        const auto original = source.raw();
        const auto one = Value::integer(1);
        std::array rawArgs{original, one.raw()};
        rawReject(other, "Add", rawArgs);
        rawArgs[0].flags |= WIO_NATIVE_ABI_VALUE_CONSUMED;
        rawReject(module, "Add", rawArgs);
        rawArgs[0] = original;
        rawArgs[0].flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_TASK_SOURCE;
        rawReject(module, "Add", rawArgs);
        rawArgs[0] = original;
        rawArgs[0].typeId ^= 1;
        rawReject(module, "Add", rawArgs);
        auto copiedSource = *static_cast<const WioNativeTaskSource*>(original.payload.slice.data);
        rawArgs[0] = original;
        rawArgs[0].payload.slice.data = &copiedSource;
        rawReject(module, "Add", rawArgs);
        auto wrongScalar = Value::boolean(false);
        rawArgs = {original, wrongScalar.raw()};
        rawReject(module, "Add", rawArgs);
        rawArgs[1] = one.raw();

        bool foreignRejected = false;
        std::thread foreign{[&]
                            {
                                try
                                {
                                    rawArgs[0] = original;
                                    rawReject(module, "Add", rawArgs, WIO_NATIVE_ABI_WRONG_THREAD);
                                    foreignRejected = true;
                                }
                                catch (...)
                                {
                                }
                            }};
        foreign.join();
        expect(foreignRejected && module.invoke("Accepted").asInteger() == 0 &&
                   other.invoke("Accepted").asInteger() == 0,
               "invalid arguments changed Wio state");

        auto result = module.invoke("Add", addArgs);
        expect(!module.taskReady(result), "pending task parameter was synchronously waited");
        source.reset();
        addArgs[0].reset();
        expect(module.waitTask(result).asInteger() == 42, "retained task parameter lost its source/result");
        expect(module.invoke("Accepted").asInteger() == 1, "valid task body did not execute once");

        auto pending = module.invoke("Pending");
        std::array relayArgs{pending.clone()};
        auto alias = module.invoke("Relay<i32>", relayArgs);
        unsigned cancelled = 0;
        module.onTaskComplete(pending,
                              [&](std::int32_t status)
                              {
                                  if (status == WIO_NATIVE_ABI_CANCELLED)
                                      ++cancelled;
                              });
        module.cancelTask(alias);
        expect(module.taskReady(pending) && cancelled == 1, "task relay cloned state instead of preserving identity");
        std::array cancelledArgs{pending.clone(), Value::integer(1)};
        auto cancelledParent = module.invoke("Add", cancelledArgs);
        rejects([&] { module.waitTask(cancelledParent); });
        rejects([&] { module.readTask(alias); });

        auto independentSource = module.invoke("Pending");
        std::array independentArgs{independentSource.clone(), Value::integer(1)};
        auto independentParent = module.invoke("Add", independentArgs);
        module.cancelTask(independentParent);
        expect(!module.taskReady(independentSource), "parent cancellation implicitly cancelled its shared input task");
        module.cancelTask(independentSource);

        auto broken = module.invoke("Broken");
        std::array faultArgs{broken.clone(), Value::integer(1)};
        auto faulted = module.invoke("Add", faultArgs);
        expect(module.waitTaskReady(faulted, std::chrono::seconds{3}), "faulted source did not settle parent");
        bool preservedFault = false;
        try
        {
            module.readTask(faulted);
        }
        catch (const std::exception& error)
        {
            preservedFault = std::string_view{error.what()}.find("bounds") != std::string_view::npos;
        }
        expect(preservedFault, "task argument fault text was lost");

        std::array stringArgs{Value::string(std::string{"A\0B", 3})};
        auto textSource = module.invoke("Message", stringArgs);
        std::array mismatched{textSource.raw(), one.raw()};
        rawReject(module, "Add", mismatched);
        std::array echoArgs{textSource.clone()};
        auto echo = module.invoke("Echo", echoArgs);
        auto stringAlias = module.invoke("Relay<string>", echoArgs);
        textSource.reset();
        echoArgs[0].reset();
        auto escaped = module.waitTask(echo);
        expect(escaped.asString() == std::string{"echo:A\0B", 8} &&
                   module.waitTask(stringAlias).asString() == std::string{"A\0B", 3},
               "string task input changed bytes");

        std::array unicodeArgs{Value::text(std::u32string{U'A', U'\0', U'\U0001f680'})};
        auto unicode = module.invoke("Unicode", unicodeArgs);
        std::array unicodeTask{unicode.clone()};
        auto decorated = module.invoke("UnicodeEcho", unicodeTask);
        auto unicodeAlias = module.invoke("Relay<text>", unicodeTask);
        expect(module.waitTask(decorated).asText() ==
                       std::u32string{U'\u27e6', U'A', U'\0', U'\U0001f680', U'\u27e7'} &&
                   module.waitTask(unicodeAlias).asText() == unicodeArgs[0].asText(),
               "Unicode task input changed scalars");

        std::array timerArgs{Value::unsignedInteger(1)};
        auto timer = module.invoke("Timer", timerArgs);
        std::array unitArgs{timer.clone()};
        auto finish = module.invoke("Finish", unitArgs);
        expect(module.waitTask(finish).asInteger() == 23, "void task parameter failed");

        auto mode = module.invoke("ModeTask");
        std::array modeArgs{mode.clone()};
        auto enumResult = module.invoke("EnumInput", modeArgs);
        rawReject(module, "FlagsInput", std::array{mode.raw()});
        expect(module.waitTask(enumResult).asInteger() == 1, "enum task input lost nominal identity/value");
        auto flags = module.invoke("FlagsTask");
        std::array flagArgs{flags.clone()};
        auto flagResult = module.invoke("FlagsInput", flagArgs);
        auto flagValue = module.waitTask(flagResult);
        expect(flagValue.raw().kind == WIO_NATIVE_ABI_U64 && flagValue.raw().payload.unsignedInteger == 3,
               "flagset task input changed its bits");

        auto abandoned = module.invoke("Pending");
        std::array abandonedArgs{abandoned.clone(), Value::integer(1)};
        auto parent = module.invoke("Add", abandonedArgs);
        unsigned settled = 0;
        module.onTaskComplete(parent, [&](std::int32_t) { ++settled; });
        abandoned.reset();
        abandonedArgs[0].reset();
        module.stop();
        expect(settled == 1 && escaped.asString() == std::string{"echo:A\0B", 8},
               "close broke task roots/result leases");
        std::array closedArgs{parent.clone(), Value::integer(1)};
        rejects([&] { other.invoke("Add", closedArgs); });
        rawReject(other, "Add", std::array{parent.raw(), one.raw()});
        module.close();
        other.close();
        std::thread release{[task = std::move(parent), value = std::move(escaped)]() mutable
                            {
                                task.reset();
                                value.reset();
                            }};
        release.join();
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 3, "shared and unsupported module paths required");
        rejects([&] { Host::open(std::filesystem::u8path(argv[2])); });
        const auto path = std::filesystem::u8path(argv[1]);
        qualify(Host::open(path), Host::open(path));
        const auto* sessions = WioStaticTaskInputsContractSessionApi();
        const auto* tasks = WioStaticTaskInputsContractTaskApi();
        expect(WioValidateSdkModuleTaskApi(tasks, sessions), "static task descriptor invalid");
        auto lifetime = std::make_shared<int>(0);
        qualify(Host::attachSession(*sessions, lifetime, tasks), Host::attachSession(*sessions, lifetime, tasks));
        std::cout << "vm-sdk-task-inputs-artifacts-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
