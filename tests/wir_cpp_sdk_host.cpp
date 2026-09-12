#include "wio_native_sdk.h"
#include <array>
#include <iostream>

int main(int argc, char** argv) {
    using namespace wio::sdk;
    if (argc!=2) return 1;
    try {
        auto module=NativeModule::open(argv[1]); module.start();
        auto* legacy=module.legacyApi();
        if (!legacy || legacy->descriptorVersion!=11 || !legacy->saveState || legacy->saveState()!=3) return 2;
        legacy->restoreState(8); if (legacy->saveState()!=8) return 3;
        WioValue oldArgs[2]; oldArgs[0].type=oldArgs[1].type=WIO_ABI_I32;
        oldArgs[0].value.v_i32=2; oldArgs[1].value.v_i32=5; WioValue oldResult;
        if (WioInvokeModuleExport(legacy,"Add",oldArgs,2,&oldResult)!=0 || oldResult.value.v_i32!=7) return 4;
        std::array args{NativeValue::integer(10),NativeValue::integer(20)};
        if (module.invoke("Add",args).asInteger()!=30) return 5;
        const auto* registry=module.nativeRegistry();
        if (!registry || !registry->functionCount) return 21;
        bool sawIdentity=false;
        for (std::uint32_t i=0;i<registry->functionCount;++i) {
            const auto& f=registry->functions[i];
            if (std::string_view(f.nativeSymbol)!="wir_native::Identity") continue;
            sawIdentity=true;
            auto input=NativeValue::integer(42); WioNativeAbiValue result; WioNativeAbiFailure failure;
            if (f.thunk(&input.raw(),1,&result,&failure)!=WIO_NATIVE_ABI_OK || result.payload.signedInteger!=42) return 22;
            WioNativeAbiReleaseValue(&result);
            if (f.thunk(nullptr,1,&result,&failure)!=WIO_NATIVE_ABI_INVALID_ARGUMENT ||
                f.thunk(&input.raw(),1,nullptr,&failure)!=WIO_NATIVE_ABI_INVALID_ARGUMENT) return 23;
            auto overflow=NativeValue::integer(0x100000000LL);
            if (f.thunk(&overflow.raw(),1,&result,&failure)!=WIO_NATIVE_ABI_TYPE_MISMATCH) return 24;
        }
        if (!sawIdentity) return 25;
        auto unicode=NativeValue::text(U"Wio \U0001f30d e\u0301");
        if (module.invoke("Unicode",std::span(&unicode,1)).asText()!=U"\u27e6Wio \U0001f30d e\u0301\u27e7") return 26;
        auto message=module.invoke("Message"); if (message.asString()!="wio dll") return 6;
        auto borrow=message.borrow(); module.invoke("Append",std::span(&borrow,1));
        if (message.asString()!="wio dll!") return 7;
        auto integer=NativeValue::integer(4); std::array aliases{integer.borrow(),integer.borrow()};
        if (module.invoke("Aliases",aliases).asInteger()!=7 || integer.asInteger()!=7) return 8;
        auto packet=module.invoke("MakePacket"); if (module.invoke("ReadPacket",std::span(&packet,1)).asInteger()!=42) return 9;
        auto object=module.invoke("MakeCounter"); auto clone=object.clone();
        if (module.invoke("ReadCounter",std::span(&clone,1)).asInteger()!=17) return 10;
        std::array same{object.clone(),clone.clone()}; if (!module.invoke("Same",same).asBoolean()) return 11;
        {
            auto owned=module.invoke("MakeCounter"); auto retained=owned.clone(); owned.reset();
            if (module.invoke("DropCount").asInteger()!=0 || module.invoke("ReadCounter",std::span(&retained,1)).asInteger()!=17) return 30;
            retained.reset(); if (module.invoke("DropCount").asInteger()!=1) return 31;
        }
        bool rejected=false; try { module.invoke("Fail"); } catch (...) { rejected=true; } if (!rejected) return 12;
        rejected=false; std::array bad{NativeValue::string("bad"),NativeValue::integer(1)};
        try { module.invoke("Add",bad); } catch (...) { rejected=true; } if (!rejected) return 13;
        auto readOnly=message.borrow(false);
        rejected=false; try { module.invoke("Append",std::span(&readOnly,1)); } catch (...) { rejected=true; }
        if (!rejected || message.asString()!="wio dll!") return 27;
        auto mutated=NativeValue::string("before"); auto mutBorrow=mutated.borrow();
        rejected=false; try { module.invoke("MutateThenFail",std::span(&mutBorrow,1)); } catch (...) { rejected=true; }
        if (!rejected || mutated.asString()!="before?") return 28;
        auto seed=NativeValue::integer(8); auto callback=module.invoke("MakeCallback",std::span(&seed,1));
        if (module.invoke("UseCallback",std::span(&callback,1)).asInteger()!=11) return 15;
        WioNativeAbiStatus threadStatus=WIO_NATIVE_ABI_OK;
        std::thread wrongThread([&] {
            auto input=NativeValue::integer(3); WioNativeAbiValue out; WioNativeAbiFailure error;
            const auto& c=callback.raw().payload.callback;
            threadStatus=c.ops->invoke(c.userdata,&input.raw(),1,&out,&error);
            WioNativeAbiReleaseValue(&out);
        });
        wrongThread.join(); if (threadStatus!=WIO_NATIVE_ABI_WRONG_THREAD) return 29;
        auto delayed=module.invoke("Delayed");
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (!module.taskReady(delayed) && std::chrono::steady_clock::now()<deadline) { module.pumpMain(); std::this_thread::yield(); }
        if (!module.taskReady(delayed) || module.readTask(delayed).asInteger()!=77) return 16;
        auto pending=module.invoke("Pending"); if (module.taskReady(pending)) return 17;
        rejected=false; try { module.readTask(pending); } catch (...) { rejected=true; } if (!rejected) return 18;
        module.cancelTask(pending);
        deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (!module.taskReady(pending) && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
        if (!module.taskReady(pending)) return 19;
        rejected=false; try { module.readTask(pending); } catch (...) { rejected=true; } if (!rejected) return 20;
        auto abandoned=module.invoke("Pending"); // Owner shutdown cancels before unloading code.
        module.close(); // Returned values still pin the DLL and owner release operations.
        if (message.asString()!="wio dll!") return 14;
        message.reset(); clone.reset(); object.reset(); packet.reset();
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 99; }
}
