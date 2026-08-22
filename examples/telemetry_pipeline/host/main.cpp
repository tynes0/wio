#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include <wio_sdk.h>

namespace
{
    enum class HostAlertLevel : std::int32_t
    {
        Normal = 0,
        Warning = 1,
        Critical = 2
    };

    enum class HostQualityFlags : std::uint32_t
    {
        None = 0u,
        Calibrated = 1u,
        Alerted = 2u,
        BaselineMissing = 4u
    };

    void require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Telemetry pipeline host expected the Wio module path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const auto info = module.inspect();
        const auto version = module.module_product_version();

        require(version.has_value(), "The module did not publish its Wio product version.");
        require(info.has_capability(WIO_MODULE_CAP_TYPE_METADATA_V2), "Type metadata v2 is unavailable.");
        require(info.has_capability(WIO_MODULE_CAP_TEXT_FIELDS), "Unicode text fields are unavailable.");

        auto ingest = module.load_command<std::int32_t(std::int32_t, float, float, std::uint32_t, float, float)>("telemetry.ingest");
        auto accepted = module.load_command<std::int32_t()>("telemetry.accepted");
        auto warnings = module.load_command<std::int32_t()>("telemetry.warnings");
        auto criticals = module.load_command<std::int32_t()>("telemetry.criticals");
        auto lastScore1000 = module.load_command<std::int32_t()>("telemetry.last-score-1000");
        auto lastFingerprint = module.load_command<std::uint32_t()>("telemetry.last-fingerprint");
        auto publishSample = module.load_event<void(std::int32_t, float, float, std::uint32_t)>("telemetry.sample");

        require(ingest(7, 20.5f, 20.0f, 1'000u, 1.0f, 0.0f) == 0, "Expected the first global sample to be normal.");
        require(ingest(7, 24.0f, 20.0f, 2'000u, 1.0f, 0.0f) == 1, "Expected the second global sample to be a warning.");
        require(ingest(9, 42.0f, 20.0f, 3'000u, 1.0f, 0.0f) == 2, "Expected the third global sample to be critical.");
        publishSample(11, 19.0f, 20.0f, 4'000u);

        require(accepted() == 4, "Global command/event accounting is inconsistent.");
        require(warnings() == 1, "Global warning accounting is inconsistent.");
        require(criticals() == 1, "Global critical accounting is inconsistent.");
        require(lastScore1000() == 500, "The event did not update the native score.");
        require(lastFingerprint() != 0u, "The native fingerprint did not cross the SDK boundary.");

        auto sessionType = module.load_object("TelemetrySession");
        auto snapshotType = module.load_component("TelemetrySnapshot");
        require(sessionType.list_fields().size() == 7u, "TelemetrySession reflection is incomplete.");
        require(snapshotType.list_fields().size() == 10u, "TelemetrySnapshot reflection is incomplete.");

        const auto titleInfo = sessionType.field_info("title");
        const auto scoresInfo = sessionType.field_info("recentScores");
        const auto bucketsInfo = sessionType.field_info("buckets");
        const auto lastInfo = sessionType.field_info("last");
        require(titleInfo.is_text() && titleInfo.supports_dynamic_value(), "The title field is not a dynamic Unicode text value.");
        require(scoresInfo.is_dynamic_array() && scoresInfo.type.element_type().is_f32(), "The score array descriptor is incomplete.");
        require(bucketsInfo.is_dict() && bucketsInfo.type.key_type().is_string() && bucketsInfo.type.value_type().is_i32(), "The bucket dictionary descriptor is incomplete.");
        require(lastInfo.is_component() && lastInfo.logical_type_name() == "TelemetrySnapshot", "The nested component descriptor is incomplete.");

        auto session = sessionType.create();
        const auto replacementTitle = wio::sdk::WioText::from_utf8("İstanbul gözlem hattı 🌍");
        session.field("title").set_dynamic(wio::sdk::WioDynamicValue(replacementTitle));

        auto process = session.method<std::int32_t(std::int32_t, float, float, std::uint32_t, float, float)>("Process");
        auto scoreSum = session.method<float()>("ScoreSum");

        require(process(101, 10.2f, 10.0f, 10'000u, 1.0f, 0.0f) == 0, "Session normal classification failed.");
        require(process(101, 12.0f, 10.0f, 11'000u, 1.0f, 0.0f) == 1, "Session warning classification failed.");
        require(process(202, 14.0f, 10.0f, 12'000u, 1.0f, 0.0f) == 2, "Session critical classification failed.");

        const auto title = session.field("title").get_text();
        const auto scores = session.get_array<float>("recentScores");
        const auto buckets = session.get_dict<wio::string, std::int32_t>("buckets");
        const auto last = session.get_component("last");
        const auto level = last.get<HostAlertLevel>("level");
        const auto flags = last.get<HostQualityFlags>("flags");

        require(title == replacementTitle, "Unicode text did not round-trip through the SDK.");
        require(session.get<std::int32_t>("accepted") == 3, "Session accepted count is incorrect.");
        require(session.get<std::int32_t>("warnings") == 1, "Session warning count is incorrect.");
        require(session.get<std::int32_t>("criticals") == 1, "Session critical count is incorrect.");
        require(scores.count() == 3u, "Dynamic score history did not cross the SDK boundary.");
        require(buckets.at("normal") == 1 && buckets.at("warning") == 1 && buckets.at("critical") == 1, "Dictionary counters did not round-trip.");
        require(last.get<std::int32_t>("sensorId") == 202, "Nested component scalar read failed.");
        require(level == HostAlertLevel::Critical, "Enum value did not cross the SDK boundary.");
        require((static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(HostQualityFlags::Alerted)) != 0u, "Flagset value did not cross the SDK boundary.");
        require(last.get<std::uint32_t>("fingerprint") != 0u, "Nested native fingerprint is missing.");

        std::cout << "Telemetry pipeline: Wio "
                  << version->major << '.' << version->minor << '.' << version->patch
                  << " | module-types=" << info.types.size()
                  << " | global=" << accepted() << '/' << warnings() << '/' << criticals()
                  << " | session=" << session.get<std::int32_t>("accepted")
                  << " | score-sum=" << std::fixed << std::setprecision(2) << scoreSum()
                  << " | title-codepoints=" << title.code_point_count()
                  << " | last-sensor=" << last.get<std::int32_t>("sensorId")
                  << " | fingerprint=" << last.get<std::uint32_t>("fingerprint")
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "Telemetry pipeline failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
