#include "wio/ast/attribute_contract.h"

#include <algorithm>
#include <array>

namespace wio
{
    namespace
    {
        using namespace std::literals;

        constexpr std::array<std::string_view, 0> none{};
        constexpr std::array nativeTargets{"fn"sv, "method"sv, "component"sv, "object"sv, "interface"sv, "enum"sv, "flagset"sv};
        constexpr std::array objectTargets{"component"sv, "object"sv};
        constexpr std::array objectInterfaceTargets{"component"sv, "object"sv, "interface"sv};
        constexpr std::array enumTargets{"enum"sv, "flagset"sv};
        constexpr std::array fieldTargets{"field"sv, "variable"sv};
        constexpr std::array functionOnlyTargets{"fn"sv};
        constexpr std::array instantiateTargets{"object"sv, "component"sv, "fn"sv, "method"sv};
        constexpr std::array applyTargets{"type"sv, "interface"sv, "object"sv, "component"sv, "fn"sv, "method"sv};
        constexpr std::array nativeOrExport{"std::attribute::Native"sv, "std::attribute::Export"sv};
        constexpr std::array nativeConflict{"std::attribute::Native"sv};
        constexpr std::array exportConflict{"std::attribute::Export"sv};
        constexpr std::array exportRequired{"std::attribute::Export"sv};
        constexpr std::array commandConflict{"std::attribute::Command"sv};
        constexpr std::array eventConflict{"std::attribute::Event"sv};

        const std::array contracts{
            BuiltinAttributeContract{Attribute::ReadOnly, "std::attribute::ReadOnly", fieldTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::Default, "std::attribute::Default", objectInterfaceTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::NoDefaultCtor, "std::attribute::NoDefaultCtor", objectTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::GenerateCtors, "std::attribute::GenerateCtors", objectTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::From, "std::attribute::From", objectTargets, none, none, none, true},
            BuiltinAttributeContract{Attribute::Trust, "std::attribute::Trust", objectTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::Final, "std::attribute::Final", objectInterfaceTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::Type, "std::attribute::Type", enumTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::Native, "std::attribute::Native", nativeTargets, none, none, exportConflict, false},
            BuiltinAttributeContract{Attribute::CppHeader, "std::attribute::CppHeader", nativeTargets, none, none, none, true},
            BuiltinAttributeContract{Attribute::CppName, "std::attribute::CppName", nativeTargets, none, nativeOrExport, none, false},
            BuiltinAttributeContract{Attribute::Instantiate, "std::attribute::Instantiate", instantiateTargets, none, none, none, true},
            BuiltinAttributeContract{Attribute::Specialize, "std::attribute::Specialize", objectTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::Apply, "std::attribute::Apply", applyTargets, none, none, none, true},
            BuiltinAttributeContract{Attribute::Export, "std::attribute::Export", instantiateTargets, none, none, nativeConflict, false},
            BuiltinAttributeContract{Attribute::Command, "std::attribute::Command", functionOnlyTargets, exportRequired, none, eventConflict, false},
            BuiltinAttributeContract{Attribute::Event, "std::attribute::Event", functionOnlyTargets, exportRequired, none, commandConflict, false},
            BuiltinAttributeContract{Attribute::ModuleApiVersion, "std::attribute::ModuleApiVersion", functionOnlyTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::ModuleLoad, "std::attribute::ModuleLoad", functionOnlyTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::ModuleUpdate, "std::attribute::ModuleUpdate", functionOnlyTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::ModuleUnload, "std::attribute::ModuleUnload", functionOnlyTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::ModuleSaveState, "std::attribute::ModuleSaveState", functionOnlyTargets, none, none, none, false},
            BuiltinAttributeContract{Attribute::ModuleRestoreState, "std::attribute::ModuleRestoreState", functionOnlyTargets, none, none, none, false}
        };

        std::string_view tail(const std::string_view name)
        {
            const size_t separator = name.rfind("::");
            return separator == std::string_view::npos ? name : name.substr(separator + 2);
        }
    }

    const BuiltinAttributeContract* getBuiltinAttributeContract(const Attribute attribute)
    {
        const auto contract = std::ranges::find(contracts, attribute, &BuiltinAttributeContract::attribute);
        return contract == contracts.end() ? nullptr : &*contract;
    }

    std::optional<Attribute> resolveBuiltinAttribute(const std::string_view name)
    {
        for (const auto& contract : contracts)
        {
            if (name == contract.canonicalName || tail(name) == tail(contract.canonicalName))
                return contract.attribute;
        }

        if (name == "readonly") return Attribute::ReadOnly;
        if (name == "default") return Attribute::Default;
        if (name == "no_default_ctor") return Attribute::NoDefaultCtor;
        if (name == "generate_ctors") return Attribute::GenerateCtors;
        if (name == "from" || name == "conversion::from") return Attribute::From;
        if (name == "trust") return Attribute::Trust;
        if (name == "final") return Attribute::Final;
        if (name == "type" || name == "abi::type") return Attribute::Type;
        if (name == "native") return Attribute::Native;
        if (name == "cpp::header") return Attribute::CppHeader;
        if (name == "cpp::name") return Attribute::CppName;
        if (name == "instantiate") return Attribute::Instantiate;
        if (name == "specialize") return Attribute::Specialize;
        if (name == "apply") return Attribute::Apply;
        if (name == "export" || name == "export::c") return Attribute::Export;
        if (name == "command") return Attribute::Command;
        if (name == "event") return Attribute::Event;
        if (name == "module::api_version") return Attribute::ModuleApiVersion;
        if (name == "module::load") return Attribute::ModuleLoad;
        if (name == "module::update") return Attribute::ModuleUpdate;
        if (name == "module::unload") return Attribute::ModuleUnload;
        if (name == "module::save_state") return Attribute::ModuleSaveState;
        if (name == "module::restore_state") return Attribute::ModuleRestoreState;
        return std::nullopt;
    }

    std::string_view canonicalBuiltinAttributeName(const Attribute attribute)
    {
        const auto* contract = getBuiltinAttributeContract(attribute);
        return contract ? contract->canonicalName : std::string_view{};
    }

    bool matchesBuiltinAttribute(const AttributeStatement& statement, const Attribute attribute)
    {
        if (statement.attribute == attribute)
            return true;

        const auto canonicalName = canonicalBuiltinAttributeName(attribute);
        return !canonicalName.empty() && statement.canonicalName == canonicalName;
    }
}
