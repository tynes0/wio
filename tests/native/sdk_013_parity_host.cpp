#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.13 parity host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const auto moduleInfo = module.inspect();
        const auto moduleVersion = module.module_product_version();
        if (!moduleVersion.has_value() ||
            moduleVersion->major != 0u || moduleVersion->minor != 13u || moduleVersion->patch != 0u ||
            moduleInfo.descriptor_version != WIO_MODULE_API_DESCRIPTOR_VERSION ||
            moduleInfo.descriptor_size != sizeof(WioModuleApi) ||
            !moduleInfo.has_capability(WIO_MODULE_CAP_PRODUCT_VERSION) ||
            !moduleInfo.has_capability(WIO_MODULE_CAP_TYPE_METADATA_V2) ||
            !moduleInfo.has_capability(WIO_MODULE_CAP_TEXT_FIELDS))
        {
            std::cerr << "Generated module did not publish the Wio 0.13 SDK contract.\n";
            return EXIT_FAILURE;
        }

        auto stateType = module.load_object("Sdk13State");
        const auto labelInfo = stateType.field_info("label");
        const auto selectedInfo = stateType.field_info("selected");
        if (!labelInfo.is_text() ||
            !labelInfo.supports_dynamic_value() ||
            labelInfo.type.stable_id() != WioStableTypeId("text") ||
            !selectedInfo.type.is_option() ||
            selectedInfo.type.logical_name() != "std::Option" ||
            selectedInfo.type.generic_argument_count() != 1u ||
            !selectedInfo.type.generic_argument(0u).is_i32() ||
            selectedInfo.type.stable_id() != WioStableTypeId("std::Option<i32>") ||
            selectedInfo.supports_dynamic_value())
        {
            std::cerr << "Generated 0.13 type metadata is incomplete.\n";
            return EXIT_FAILURE;
        }
        const std::string selectedTypeName(selectedInfo.type.name());

        auto state = stateType.create();
        auto label = state.field("label");
        if (label.get_text() != wio::sdk::WioText::from_utf8("Başlangıç 🚀"))
        {
            std::cerr << "Initial Wio text did not cross the SDK boundary.\n";
            return EXIT_FAILURE;
        }

        const auto replacement = wio::sdk::WioText::from_utf8("İstanbul 🌍");
        label.set_dynamic(wio::sdk::WioDynamicValue(replacement));
        const auto dynamicLabel = label.get_dynamic();
        if (!dynamicLabel.is_text() || dynamicLabel.as_text() != replacement)
        {
            std::cerr << "Wio text did not round-trip through dynamic SDK access.\n";
            return EXIT_FAILURE;
        }

        // ModuleInfo owns these names; the snapshot remains useful after unload.
        module.close();
        if (moduleInfo.types.size() != 1u || moduleInfo.types.front() != "Sdk13State")
        {
            std::cerr << "Module inspection snapshot did not retain owned metadata.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.13 parity: version=0.13.0 text-codepoints="
                  << dynamicLabel.as_text().code_point_count()
                  << " generic=" << selectedTypeName;
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
