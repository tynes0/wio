#include "wio/wir/module.h"

namespace wio::wir
{
    std::string_view moduleKindName(const ModuleKind kind)
    {
        switch (kind)
        {
        case ModuleKind::Program: return "program";
        case ModuleKind::WioLibrary: return "wio-library";
        case ModuleKind::NativeLibrary: return "native-library";
        }
        return "unknown";
    }

    std::string_view moduleImportKindName(const ModuleImportKind kind)
    {
        switch (kind)
        {
        case ModuleImportKind::WioModule: return "wio-module";
        case ModuleImportKind::StandardModule: return "standard-module";
        case ModuleImportKind::NativeHeader: return "native-header";
        case ModuleImportKind::NativeLibrary: return "native-library";
        }
        return "unknown";
    }

    std::string_view moduleExportKindName(const ModuleExportKind kind)
    {
        switch (kind)
        {
        case ModuleExportKind::Function: return "function";
        case ModuleExportKind::GenericFunctionSpecialization: return "generic-function-specialization";
        case ModuleExportKind::ObjectType: return "object-type";
        case ModuleExportKind::ComponentType: return "component-type";
        }
        return "unknown";
    }

    std::string_view moduleExportRoleName(const ModuleExportRole role)
    {
        switch (role)
        {
        case ModuleExportRole::Ordinary: return "ordinary";
        case ModuleExportRole::Command: return "command";
        case ModuleExportRole::EventHook: return "event-hook";
        }
        return "unknown";
    }
}
