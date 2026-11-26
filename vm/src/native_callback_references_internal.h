#pragma once

#include "native_bridge_internal.h"
#include "place_storage_internal.h"

#include <deque>

namespace wio::vm::detail
{
    // A callback invocation owns its borrow materialization. No VM place or
    // native stack address is installed in a retained callback's state.
    class NativeCallbackReferences final
    {
    public:
        explicit NativeCallbackReferences(const NativeObjectAccess* access = nullptr) : access_(access)
        {
        }
        NativeCallbackReferences(const NativeCallbackReferences&) = delete;
        NativeCallbackReferences& operator=(const NativeCallbackReferences&) = delete;
        ~NativeCallbackReferences();
        NativeValueResult read(const bytecode::Module& module, const WioNativeAbiValue& wire, std::uint32_t type,
                               const NativePodLayouts* layouts);
        bool write(const bytecode::Module& module, const Value& value, std::uint32_t type, WioNativeAbiValue& wire,
                   std::string& error, const NativePodLayouts* layouts);
        bool commit(const bytecode::Module& module, std::string& error, const NativePodLayouts* layouts);
        NativeValueResult readResult(const bytecode::Module&, const WioNativeAbiValue&, std::uint32_t);
        bool writeResult(const bytecode::Module&, const Value&, std::uint32_t, WioNativeAbiValue&, std::string&);
        // Keep materializations alive until the callback result has been encoded.
        bool finish(std::string& error);

    private:
        struct Slot
        {
            WioNativeAbiValue* incoming = nullptr;
            WioNativeAbiValue outgoing{};
            PlaceStorage* original = nullptr;
            std::unique_ptr<PlaceStorage> local;
            std::uint32_t type = 0;
            std::uint8_t marshalling = 0;
            bool mutableAccess = false;
        };
        std::deque<Slot> slots_;
        const NativeObjectAccess* access_;
    };
} // namespace wio::vm::detail
