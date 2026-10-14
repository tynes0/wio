#include <wio/vm/module_adapter.h>
#include <wio_native_sdk.h>

#include <iostream>
#include <functional>
#include <stdexcept>
#include <thread>

namespace
{
    namespace bc = wio::bytecode;
    using Adapter = wio::vm::ModuleAdapter;

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    bc::Module fixture()
    {
        bc::Module module;
        module.moduleKind = WIO_SDK_MODULE_WIO_LIBRARY;
        module.abiDescriptorVersion = 11;
        module.strings = {"vm-sdk-test",
                          "wio.module:vm-sdk-test",
                          "",
                          "Integer",
                          "String",
                          "Text",
                          "Byte",
                          "i32",
                          "string",
                          "text",
                          "i8",
                          "function:Integer",
                          "function:String",
                          "function:Text",
                          "function:Byte"};
        module.name = module.logicalName = 0;
        module.stableKey = 1;
        module.stableId = WioStableSdkId(module.strings[1].c_str());
        module.contract.callTableStableId = 1;
        module.constants.emplace_back();
        module.types = {bc::Type{.kind = 5, .name = 7}, bc::Type{.kind = 17, .name = 8},
                        bc::Type{.kind = 18, .name = 9}, bc::Type{.kind = 3, .name = 10}};
        for (std::uint32_t index = 0; index < 4; ++index)
        {
            bc::Function function;
            function.id = index;
            function.name = index + 3;
            function.returnType = index;
            function.parameters.push_back(bc::Parameter{.value = 0, .name = 2, .type = index});
            bc::Block block;
            block.id = 0;
            block.instructions.push_back(bc::Instruction{.opcode = bc::Opcode::Return, .operands = {0}, .constant = 0});
            function.blocks.push_back(std::move(block));
            module.functions.push_back(std::move(function));
            bc::Export entry;
            entry.stableKey = index + 11;
            entry.stableId = WioStableSdkId(module.strings[entry.stableKey].c_str());
            entry.logicalName = entry.symbolName = index + 3;
            entry.roleName = 2;
            entry.function = index;
            entry.parameterTypes = {index};
            entry.returnType = index;
            entry.callTableSlot = index;
            module.contract.callTableEntries.push_back(entry.stableId);
            module.contract.exports.push_back(std::move(entry));
        }
        return module;
    }

    template <class F> void rejects(F action)
    {
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            return;
        }
        throw std::runtime_error("unsupported or invalid module was accepted");
    }

    class ReentryObserver final : public wio::vm::DebugObserver
    {
    public:
        std::function<void()> attempt;

        wio::vm::DebugAction onInstruction(const wio::vm::DebugEvent&) override
        {
            if (attempt)
            {
                auto action = std::move(attempt);
                action();
            }
            return wio::vm::DebugAction::Continue;
        }
    };
} // namespace

int main()
{
    try
    {
        auto adapter = std::make_shared<Adapter>(fixture());
        const auto& contract = adapter->contract();
        expect(WioValidateSdkModuleContract(&contract) && contract.exportCount == 4, "invalid VM SDK descriptor");
        const auto call =
            [&](std::size_t index, const WioNativeAbiValue* input, std::uint32_t count, WioNativeAbiValue* result)
        {
            const auto& entry = contract.callTable[index];
            return entry.invoke(input, count, result, entry.context);
        };
        WioNativeAbiValue input{}, result{};
        input.kind = WIO_NATIVE_ABI_I64;
        input.payload.signedInteger = 42;
        expect(call(0, &input, 1, &result) == WIO_NATIVE_ABI_OK && result.payload.signedInteger == 42,
               "scalar export dispatch failed");
        expect(call(0, &input, 1, &result) == WIO_NATIVE_ABI_INVALID_ARGUMENT && result.payload.signedInteger == 42,
               "nonempty output must not be overwritten");
        WioNativeAbiReleaseValue(&result);
        expect(call(0, nullptr, 1, &result) == WIO_NATIVE_ABI_INVALID_ARGUMENT, "null argument array accepted");
        expect(call(0, &input, 0, &result) == WIO_NATIVE_ABI_INVALID_ARGUMENT, "wrong arity accepted");
        expect(call(0, &input, 1, nullptr) == WIO_NATIVE_ABI_INVALID_ARGUMENT, "missing result accepted");
        input.kind = WIO_NATIVE_ABI_U64;
        expect(call(0, &input, 1, &result) == WIO_NATIVE_ABI_TYPE_MISMATCH, "wrong scalar kind accepted");
        input.kind = WIO_NATIVE_ABI_I64;
        input.payload.signedInteger = 128;
        expect(call(3, &input, 1, &result) == WIO_NATIVE_ABI_TYPE_MISMATCH && !adapter->lastError().empty(),
               "narrow scalar overflow accepted");
        input.flags = WIO_NATIVE_ABI_VALUE_CONSUMED;
        expect(call(0, &input, 1, &result) == WIO_NATIVE_ABI_TYPE_MISMATCH, "consumed input accepted");
        input.flags = 0;
        std::int32_t foreignStatus = 0;
        std::thread foreign{[&] { foreignStatus = call(0, &input, 1, &result); }};
        foreign.join();
        expect(foreignStatus == WIO_NATIVE_ABI_WRONG_THREAD && result.kind == WIO_NATIVE_ABI_VOID,
               "foreign-thread entry did not reject safely");

        auto host = wio::sdk::NativeModule::attach(contract, adapter);
        std::weak_ptr<Adapter> weak = adapter;
        adapter.reset();
        const std::string bytes{"hello\0world", 11};
        std::vector<wio::sdk::NativeValue> arguments;
        arguments.push_back(wio::sdk::NativeValue::string(bytes));
        auto escaped = host.invoke("String", arguments);
        expect(escaped.asString() == bytes && escaped.raw().owner.state &&
                   (escaped.raw().flags & WIO_NATIVE_ABI_VALUE_OWNED),
               "returned string is not an owning byte-exact result");
        auto clone = escaped.clone();
        arguments.clear();
        arguments.push_back(wio::sdk::NativeValue::text(std::u32string{U'A', U'\0', U'\U0001f680'}));
        auto text = host.invoke("Text", arguments);
        expect(text.asText() == std::u32string{U'A', U'\0', U'\U0001f680'}, "Unicode result changed scalars");
        host.close();
        expect(!weak.expired() && escaped.asString() == bytes && clone.asString() == bytes,
               "SDK close released a module retained by its result");
        text.reset();
        escaped.reset();
        expect(!weak.expired(), "cloned result did not retain the adapter");
        clone.reset();
        expect(weak.expired(), "last SDK result leaked adapter ownership");

        // Raw owning wire storage outlives the adapter itself; DLL code must
        // still remain mapped until the owner callback has run.
        adapter = std::make_shared<Adapter>(fixture());
        const auto& raw = adapter->contract().callTable[2];
        const char32_t invalidScalar = 0xd800;
        input = {};
        input.kind = WIO_NATIVE_ABI_TEXT_UTF32;
        input.payload.slice = {&invalidScalar, sizeof(invalidScalar)};
        expect(raw.invoke(&input, 1, &result, raw.context) == WIO_NATIVE_ABI_TYPE_MISMATCH,
               "invalid Unicode scalar accepted");
        const std::u32string unicode{U'\0', U'\U0001f680'};
        input.payload.slice = {unicode.data(), unicode.size() * sizeof(char32_t)};
        expect(raw.invoke(&input, 1, &result, raw.context) == WIO_NATIVE_ABI_OK, "raw Unicode export failed");
        adapter.reset();
        const auto* scalars = static_cast<const char32_t*>(result.payload.slice.data);
        expect(scalars[0] == U'\0' && scalars[1] == U'\U0001f680', "result storage died with adapter");
        WioNativeAbiReleaseValue(&result);

        ReentryObserver observer;
        adapter = std::make_shared<Adapter>(fixture(), wio::vm::MachineOptions{.debugObserver = &observer});
        const auto& observed = adapter->contract().callTable[0];
        std::int32_t reentry = 0;
        observer.attempt = [&]
        {
            WioNativeAbiValue nested{};
            reentry = observed.invoke(nullptr, 0, &nested, observed.context);
        };
        input = {};
        input.kind = WIO_NATIVE_ABI_I64;
        input.payload.signedInteger = 42;
        expect(observed.invoke(&input, 1, &result, observed.context) == WIO_NATIVE_ABI_OK &&
                   reentry == WIO_NATIVE_ABI_INVALID_ARGUMENT && result.payload.signedInteger == 42,
               "reentrant host call was not isolated from its outer invocation");
        WioNativeAbiReleaseValue(&result);
        adapter.reset();

        rejects(
            []
            {
                auto m = fixture();
                m.contract.callTableEntries.clear();
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.contract.exports[0].callTableSlot = 9;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.contract.exports[0].isAsync = true;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.contract.exports[0].parameterTypes[0] = 1;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.moduleKind = 0;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.stableId ^= 1;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.strings[3].push_back('\0');
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.contract.lifecycle.load = 0;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.types[0].kind = 29;
                m.types[0].arguments = {3};
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.types[0].kind = 30;
                m.types[0].arguments = {3};
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.types[0].kind = 31;
                m.types[0].arguments = {3};
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.contract.exports[0].kind = WIO_SDK_EXPORT_COMPONENT_TYPE;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                auto m = fixture();
                m.functions[0].blocks[0].instructions[0].asyncOperation = 1;
                Adapter a{std::move(m)};
            });
        rejects(
            []
            {
                Adapter a{fixture()};
                auto host = wio::sdk::NativeModule::attach(a.contract(), {});
            });
        std::cout << "vm-sdk-module-adapter-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
