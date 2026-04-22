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
            !titleField.supports_dynamic_value() ||
            !titleField.is_string() ||
            !tagsField.is_array() ||
            !tagsField.is_dynamic_array() ||
            !tagsField.supports_dynamic_value() ||
            !tagsField.type.has_element_type() ||
            !tagsField.type.element_type().is_string() ||
            !scoresField.is_dict() ||
            !scoresField.supports_dynamic_value() ||
            !scoresField.type.has_key_type() ||
            !scoresField.type.key_type().is_string() ||
            !scoresField.type.has_value_type() ||
            !scoresField.type.value_type().is_i32() ||
            !orderField.is_tree() ||
            !orderField.supports_dynamic_value() ||
            !orderField.type.key_type().is_string() ||
            !orderField.type.value_type().is_integer() ||
            !fixedField.is_static_array() ||
            !fixedField.supports_dynamic_value() ||
            fixedField.type.static_extent() != 3u ||
            !fixedField.type.element_type().is_i32() ||
            !profileField.is_object() ||
            !profileField.supports_dynamic_value() ||
            profileField.logical_type_name() != "Profile" ||
            !positionField.is_component() ||
            !positionField.supports_dynamic_value() ||
            positionField.logical_type_name() != "Position" ||
            !callbackField.is_function() ||
            !callbackField.supports_dynamic_value() ||
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
        auto titleAccessor = state.field("title");
        auto tagsAccessor = state.field("tags");
        auto callbackAccessor = state.field("callback");
        auto title = state.get<wio::string>("title");
        auto tags = state.get_array<wio::string>("tags");
        auto scores = state.get_dict<wio::string, std::int32_t>("scores");
        auto order = state.get_tree<wio::string, std::int32_t>("order");
        auto fixed = state.get_static_array<std::int32_t, 3>("fixed");

        auto profile = state.get_object("profile");
        auto position = state.get_component("position");

        if (!titleAccessor ||
            !state.owns_handle() ||
            !profile.is_borrowed() ||
            !position.is_borrowed() ||
            state.list_fields().size() != fields.size() ||
            titleAccessor.name() != "title" ||
            !titleAccessor.can_access_as<wio::string>() ||
            titleAccessor.can_access_as<std::int32_t>() ||
            !titleAccessor.supports_dynamic_value() ||
            !tagsAccessor.supports_dynamic_value() ||
            !callbackAccessor.supports_dynamic_value() ||
            titleAccessor.get_string() != "arena" ||
            tagsAccessor.get_array<wio::string>().count() != 3u ||
            !callbackAccessor.can_access_as<std::function<std::int32_t(std::int32_t)>>() ||
            title != "arena" ||
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

        auto dynamicTitle = titleAccessor.get_dynamic();
        auto dynamicTags = tagsAccessor.get_dynamic();
        auto dynamicProfile = state.field("profile").get_dynamic();
        auto dynamicPosition = state.field("position").get_dynamic();
        auto dynamicCallback = callbackAccessor.get_dynamic();

        if (!dynamicTitle.is_string() ||
            dynamicTitle.as_string() != "arena" ||
            !dynamicTags.is_dynamic_array() ||
            dynamicTags.as_dynamic_array().as_array<wio::string>().count() != 3u ||
            !dynamicProfile.is_object() ||
            dynamicProfile.as_object().get<std::int32_t>("level") != 9 ||
            !dynamicPosition.is_component() ||
            dynamicPosition.as_component().get<std::int32_t>("y") != 7 ||
            !dynamicCallback.is_function() ||
            dynamicCallback.as_dynamic_function().as_function<std::int32_t(std::int32_t)>().valid())
        {
            std::cerr << "Dynamic field access did not expose the expected initial values." << '\n';
            return EXIT_FAILURE;
        }

        bool mismatchedFieldReadRejected = false;
        try
        {
            (void)titleAccessor.get_as<std::int32_t>();
        }
        catch (const wio::sdk::Error& error)
        {
            mismatchedFieldReadRejected = error.code() == wio::sdk::ErrorCode::SignatureMismatch;
        }

        if (!mismatchedFieldReadRejected)
        {
            std::cerr << "Field accessor accepted an invalid host type read." << '\n';
            return EXIT_FAILURE;
        }

        bool mismatchedDynamicAssignRejected = false;
        try
        {
            callbackAccessor.set_dynamic(wio::sdk::WioDynamicValue(wio::DArray<wio::string>{ "oops" }));
        }
        catch (const wio::sdk::Error& error)
        {
            mismatchedDynamicAssignRejected = error.code() == wio::sdk::ErrorCode::SignatureMismatch;
        }

        if (!mismatchedDynamicAssignRejected)
        {
            std::cerr << "Dynamic field assignment accepted an invalid host value kind." << '\n';
            return EXIT_FAILURE;
        }

        auto positionXAccessor = position.field("x");
        const WioValue initialPositionX = positionXAccessor.get_scalar_value();
        if (initialPositionX.type != WIO_ABI_I32 || initialPositionX.value.v_i32 != 3)
        {
            std::cerr << "Scalar field accessor did not expose the expected ABI value." << '\n';
            return EXIT_FAILURE;
        }

        WioValue adjustedPositionX{};
        adjustedPositionX.type = WIO_ABI_I32;
        adjustedPositionX.value.v_i32 = 5;
        positionXAccessor.set_scalar_value(adjustedPositionX);

        if (position.get<std::int32_t>("x") != 5)
        {
            std::cerr << "Scalar field accessor did not update the component field." << '\n';
            return EXIT_FAILURE;
        }

        auto profileType = module.load_object("Profile");
        titleAccessor.set_dynamic(wio::sdk::WioDynamicValue(wio::string("captain")));
        position.field("y").set_dynamic(wio::sdk::WioDynamicValue(std::int32_t{ 13 }));
        tagsAccessor.set_dynamic(wio::sdk::WioDynamicValue(wio::DArray<wio::string>{ "left", "right" }));
        state.field("fixed").set_dynamic(wio::sdk::WioDynamicValue(wio::SArray<std::int32_t, 3>{ 6, 7, 8 }));
        state.field("scores").set_dynamic(wio::sdk::WioDynamicValue(wio::Dict<wio::string, std::int32_t>{ {"hp", 22}, {"mp", 9} }));
        state.field("order").set_dynamic(wio::sdk::WioDynamicValue(wio::Tree<wio::string, std::int32_t>{ {"iron", 1}, {"steel", 2} }));
        callbackAccessor.set_dynamic(wio::sdk::WioDynamicValue(std::function<std::int32_t(std::int32_t)>([](const std::int32_t value)
        {
            return value + 5;
        })));

        auto dynamicReplacementProfile = profileType.create();
        dynamicReplacementProfile.set("level", 55);
        dynamicReplacementProfile.set("title", wio::string("veteran"));
        state.field("profile").set_dynamic(wio::sdk::WioDynamicValue(std::move(dynamicReplacementProfile)));

        auto afterDynamicProfile = state.get_object("profile");
        auto afterDynamicTags = state.field("tags").get_dynamic();
        auto afterDynamicFixed = state.field("fixed").get_dynamic();
        auto afterDynamicScores = state.field("scores").get_dynamic();
        auto afterDynamicOrder = state.field("order").get_dynamic();
        auto afterDynamicCallback = callbackAccessor.get_dynamic();
        if (state.get<wio::string>("title") != "captain" ||
            state.method<std::int32_t(std::int32_t)>("RunCallback")(7) != 12 ||
            state.get_component("position").get<std::int32_t>("y") != 13 ||
            !afterDynamicTags.is_dynamic_array() ||
            afterDynamicTags.as_dynamic_array().as_array<wio::string>().count() != 2u ||
            !afterDynamicFixed.is_static_array() ||
            afterDynamicFixed.as_dynamic_static_array().as_array<std::int32_t, 3>()[2] != 8 ||
            !afterDynamicScores.is_dict() ||
            afterDynamicScores.as_dynamic_dict().as_dict<wio::string, std::int32_t>().at("hp") != 22 ||
            !afterDynamicOrder.is_tree() ||
            afterDynamicOrder.as_dynamic_tree().as_tree<wio::string, std::int32_t>().at("steel") != 2 ||
            !afterDynamicCallback.is_function() ||
            afterDynamicCallback.as_dynamic_function().as_function<std::int32_t(std::int32_t)>()(9) != 14 ||
            afterDynamicProfile.get<std::int32_t>("level") != 55 ||
            afterDynamicProfile.get<wio::string>("title") != "veteran")
        {
            std::cerr << "Dynamic field assignment did not update exported state." << '\n';
            return EXIT_FAILURE;
        }

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
