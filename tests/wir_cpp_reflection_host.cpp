#include <wio_native_sdk.h>
#include <iostream>

int main(int argc, char** argv) {
    using namespace wio::sdk;
    if (argc != 2)
        return 1;
    try {
        auto module = NativeModule::open(argv[1]);
        module.start();
        if (!module.invoke("CheckTraits").asBoolean())
            return 12;
        const auto& counter = module.type("Counter");
        if (!counter.exported || counter.fieldCount != 2 || counter.methodCount != 2 || counter.constructorCount != 1) {
            std::cerr << "Counter layout: " << counter.exported << ' ' << counter.fieldCount << ' '
                      << counter.methodCount << ' ' << counter.constructorCount << '\n';
            return 2;
        }
        auto initial = NativeValue::integer(12);
        auto value = module.construct("Counter", std::span(&initial, 1));
        if (module.getField(value, "value").asInteger() != 12)
            return 3;
        auto input = NativeValue::integer(5);
        module.setField(value, "value", input);
        if (module.callMethod(value, "Add", std::span(&input, 1)).asInteger() != 10)
            return 4;
        bool rejected = false;
        try {
            module.getField(value, "secret");
        } catch (...) {
            rejected = true;
        }
        if (!rejected)
            return 5;
        rejected = false;
        try {
            module.callMethod(value, "Secret");
        } catch (...) {
            rejected = true;
        }
        if (!rejected)
            return 6;
        auto packet = module.invoke("NewPacket");
        module.setField(packet, "count", input);
        if (module.getField(packet, "count").asInteger() != 5)
            return 7;
        auto alias = value.clone();
        value.reset();
        if (module.getField(alias, "value").asInteger() != 10)
            return 8;
        const auto& mode = module.type("Mode");
        if (mode.caseCount != 2 || mode.cases[0].rawValue != 7)
            return 9;
        bool found = false;
        bool field = false;
        const auto* reflection = module.reflection();
        if (module.types().size() != reflection->typeCount)
            return 13;
        for (std::uint32_t i = 0; i < reflection->attributeCount; ++i) {
            const auto& a = reflection->attributes[i];
            if (std::string_view(a.name).find("InternalOnly") != std::string_view::npos)
                return 10;
            if (a.targetId == counter.stableId && a.argumentCount &&
                std::string_view(a.arguments[0].value) == "counter\nmetadata")
                found = true;
            if (a.targetId == counter.fields[0].stableId)
                field = true;
        }
        if (!found || !field || module.attributesFor(counter.stableId).size() != 1)
            return 11;
        module.close();
        alias.reset();
        packet.reset();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 99;
    }
}
