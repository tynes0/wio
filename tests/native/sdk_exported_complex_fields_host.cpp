#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include <module_api.h>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK exported complex fields host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);

        auto complexType = module.load_object("ComplexState");
        const auto fields = complexType.list_fields();
        const auto titleField = complexType.field_info("title");
        const auto tagsField = complexType.field_info("tags");
        const auto scoresField = complexType.field_info("scores");
        const auto orderField = complexType.field_info("order");
        const auto fixedField = complexType.field_info("fixed");
        const auto profileField = complexType.field_info("profile");
        const auto positionField = complexType.field_info("position");
        const auto callbackField = complexType.field_info("callback");

        if (fields.size() != 8u ||
            titleField.access != wio::sdk::FieldAccess::Public ||
            !titleField.can_read() ||
            !titleField.can_write() ||
            !titleField.is_string() ||
            !tagsField.is_array() ||
            !tagsField.is_dynamic_array() ||
            !tagsField.type.has_element_type() ||
            !tagsField.type.element_type().is_string() ||
            !scoresField.is_dict() ||
            !scoresField.type.has_key_type() ||
            !scoresField.type.key_type().is_string() ||
            !scoresField.type.has_value_type() ||
            !scoresField.type.value_type().is_i32() ||
            !orderField.is_tree() ||
            !orderField.type.key_type().is_string() ||
            !orderField.type.value_type().is_integer() ||
            !fixedField.is_static_array() ||
            fixedField.type.static_extent() != 3u ||
            !fixedField.type.element_type().is_i32() ||
            !profileField.is_object() ||
            profileField.logical_type_name() != "Profile" ||
            !positionField.is_component() ||
            positionField.logical_type_name() != "Position" ||
            !callbackField.is_function() ||
            !callbackField.type.has_return_type() ||
            !callbackField.type.return_type().is_i32() ||
            callbackField.type.parameter_count() != 1u ||
            !callbackField.type.has_parameter_types() ||
            !callbackField.type.parameter_type(0).is_numeric() ||
            !callbackField.type.parameter_type(0).is_i32())
        {
            std::cerr << "Exported complex field reflection metadata is incomplete." << '\n';
            return EXIT_FAILURE;
        }

        auto state = complexType.create();
        auto title = state.get<wio::string>("title");
        auto tags = state.get_array<wio::string>("tags");
        auto scores = state.get_dict<wio::string, std::int32_t>("scores");
        auto order = state.get_tree<wio::string, std::int32_t>("order");
        auto fixed = state.get_static_array<std::int32_t, 3>("fixed");

        auto profile = state.get_object("profile");
        auto position = state.get_component("position");

        if (title != "arena" ||
            tags.count() != 3u ||
            scores.at("hp") != 10 ||
            scores.at("mp") != 4 ||
            order.at("bronze") != 1 ||
            order.at("silver") != 2 ||
            fixed[0] != 3 ||
            fixed[1] != 4 ||
            fixed[2] != 5 ||
            profile.get<std::int32_t>("level") != 9 ||
            position.get<std::int32_t>("x") != 3)
        {
            std::cerr << "Initial complex field values did not round-trip through the SDK." << '\n';
            return EXIT_FAILURE;
        }

        auto profileType = module.load_object("Profile");
        auto replacementProfile = profileType.create();
        replacementProfile.set("level", 77);
        replacementProfile.set("title", wio::string("elite"));
        state.set_object("profile", replacementProfile);

        auto positionType = module.load_component("Position");
        auto replacementPosition = positionType.create();
        replacementPosition.set("x", 11);
        replacementPosition.set("y", 22);
        state.set_component("position", replacementPosition);

        state.set("title", wio::string("boss"));
        state.set_array("tags", wio::sdk::WioArray<wio::string>{ "alpha", "beta" });
        state.set_dict("scores", wio::sdk::WioDict<wio::string, std::int32_t>{ {"hp", 30}, {"mp", 8} });
        state.set_tree("order", wio::sdk::WioTree<wio::string, std::int32_t>{ {"gold", 3}, {"platinum", 4} });
        state.set_static_array("fixed", wio::sdk::WioStaticArray<std::int32_t, 3>{ 9, 8, 7 });
        state.set_function<std::int32_t(std::int32_t)>("callback", wio::sdk::WioFunction<std::int32_t(std::int32_t)>([](const std::int32_t value)
        {
            return value * 3;
        }));

        auto runCallback = state.method<std::int32_t(std::int32_t)>("RunCallback");
        const std::int32_t callbackResult = runCallback(7);
        auto callback = state.get_function<std::int32_t(std::int32_t)>("callback");

        auto updatedTags = state.get_array<wio::string>("tags");
        auto updatedScores = state.get_dict<wio::string, std::int32_t>("scores");
        auto updatedOrder = state.get_tree<wio::string, std::int32_t>("order");
        auto updatedFixed = state.get_static_array<std::int32_t, 3>("fixed");
        auto updatedProfile = state.get_object("profile");
        auto updatedPosition = state.get_component("position");

        if (state.get<wio::string>("title") != "boss" ||
            updatedTags.count() != 2u ||
            updatedScores.at("hp") != 30 ||
            updatedScores.at("mp") != 8 ||
            updatedOrder.at("gold") != 3 ||
            updatedOrder.at("platinum") != 4 ||
            updatedFixed[0] != 9 ||
            updatedFixed[1] != 8 ||
            updatedFixed[2] != 7 ||
            updatedProfile.get<std::int32_t>("level") != 77 ||
            updatedProfile.get<wio::string>("title") != "elite" ||
            updatedPosition.get<std::int32_t>("x") != 11 ||
            updatedPosition.get<std::int32_t>("y") != 22 ||
            callbackResult != 21 ||
            !callback.valid() ||
            callback(5) != 15)
        {
            std::cerr << "Updated complex field values did not round-trip through the SDK." << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "SDK complex fields: fields=" << fields.size()
                  << " title=" << state.get<wio::string>("title")
                  << " tags=" << updatedTags.count()
                  << " hp=" << updatedScores.at("hp")
                  << " callback=" << callbackResult
                  << " profile=" << updatedProfile.get<std::int32_t>("level")
                  << " position=" << updatedPosition.get<std::int32_t>("x")
                  << " fixedSum=" << (updatedFixed[0] + updatedFixed[1] + updatedFixed[2])
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
