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

    std::string_view metadataTargetKindName(const MetadataTargetKind kind)
    {
        switch (kind)
        {
        case MetadataTargetKind::Module: return "module";
        case MetadataTargetKind::Function: return "function";
        case MetadataTargetKind::Type: return "type";
        case MetadataTargetKind::Field: return "field";
        case MetadataTargetKind::Method: return "method";
        case MetadataTargetKind::Parameter: return "parameter";
        case MetadataTargetKind::EnumCase: return "enum-case";
        case MetadataTargetKind::Application: return "application";
        case MetadataTargetKind::System: return "system";
        case MetadataTargetKind::Handler: return "handler";
        }
        return "unknown";
    }

    std::string_view attributeOriginKindName(const AttributeOriginKind origin)
    {
        switch (origin)
        {
        case AttributeOriginKind::Direct: return "direct";
        case AttributeOriginKind::Inherited: return "inherited";
        case AttributeOriginKind::Scoped: return "scoped";
        case AttributeOriginKind::Composed: return "composed";
        case AttributeOriginKind::Generated: return "generated";
        case AttributeOriginKind::Compiler: return "compiler";
        }
        return "unknown";
    }

    std::string_view attributeProcessorPhaseName(const AttributeProcessorPhase phase)
    {
        switch (phase)
        {
        case AttributeProcessorPhase::Validation: return "validation";
        case AttributeProcessorPhase::Derive: return "derive";
        case AttributeProcessorPhase::Pre: return "pre";
        case AttributeProcessorPhase::Post: return "post";
        case AttributeProcessorPhase::Finally: return "finally";
        case AttributeProcessorPhase::Around: return "around";
        case AttributeProcessorPhase::Unknown: return "unknown";
        }
        return "unknown";
    }

    std::string_view applicationStageKindName(const ApplicationStageKind kind)
    {
        return kind == ApplicationStageKind::Fixed ? "fixed" : "variable";
    }

    std::string_view applicationAffinityName(const ApplicationAffinity affinity)
    {
        switch (affinity)
        {
        case ApplicationAffinity::Inherit: return "inherit";
        case ApplicationAffinity::Main: return "main";
        case ApplicationAffinity::Worker: return "worker";
        }
        return "unknown";
    }

    std::string_view resourceAccessName(const ResourceAccess access)
    {
        return access == ResourceAccess::Write ? "write" : "read";
    }
}
