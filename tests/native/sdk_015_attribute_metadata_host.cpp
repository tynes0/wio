#include <cstdlib>
#include <iostream>
#include <string_view>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
        return EXIT_FAILURE;
    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const WioModuleApi* api = module.raw_api();
        if (api == nullptr || api->descriptorVersion != 9u ||
            (api->capabilities & WIO_MODULE_CAP_ATTRIBUTE_METADATA_V1) == 0u)
            return EXIT_FAILURE;

        const WioModuleType* profile = WioFindModuleType(api, "Profile");
        const WioModuleAttributeDescriptor* label = profile
            ? WioFindModuleAttribute(profile->attributes, profile->attributeCount, "Label")
            : nullptr;
        const WioModuleField* value = WioFindModuleField(profile, "value");
        const WioModuleAttributeDescriptor* important = value
            ? WioFindModuleAttribute(value->attributes, value->attributeCount, "Important")
            : nullptr;
        const WioModuleMethod* read = WioFindModuleMethod(profile, "Read");
        const WioModuleAttributeDescriptor* audited = read
            ? WioFindModuleAttribute(read->attributes, read->attributeCount, "Audited")
            : nullptr;
        if (label == nullptr || std::string_view(label->argumentText) != "value=profile" ||
            important == nullptr || audited == nullptr || audited->processorCount != 1u ||
            audited->processors == nullptr ||
            audited->processors[0].phase != WIO_MODULE_ATTRIBUTE_PHASE_PRE ||
            std::string_view(audited->processors[0].hookMode) != "no_args")
            return EXIT_FAILURE;

        std::cout << "SDK 0.15 attributes: abi=9 type=Label field=Important method=Audited phase=pre\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
