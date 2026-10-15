#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

// Synthetic protocol fixture ONLY. Reported durations/checksums are deliberately
// fake so controller correctness never depends on CI machine performance.
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        return 2;
    }

    const char* setting = std::getenv("WIO_COST_FIXTURE_MODE");
    const std::string mode = setting ? setting : "steady";
    const std::string variant = std::filesystem::path(argv[0]).stem().string();
    if (mode == "timeout")
    {
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    if (mode == "large")
    {
        std::cout << std::string(70'000, 'o');
        std::cerr << std::string(70'000, 'e');
        return 0;
    }

    if (mode == "malformed")
    {
        std::cout << "not a measurement\n";
        return 0;
    }

    if (mode == "mutation")
    {
        std::ofstream changed("probe.h", std::ios::app);
        changed << "changed\n";
    }

    std::string seconds = "0.001000000";
    if (mode == "unstable")
    {
        const std::string state = "fixture-" + std::string(argv[1]) + "-" + variant + ".state";
        unsigned count = 0;
        {
            std::ifstream input(state);
            input >> count;
        }

        {
            std::ofstream output(state);
            output << count + 1;
        }

        if (count % 2)
        {
            seconds = "0.003000000";
        }
    }

    const auto checksum = mode == "mismatch" && variant == "wio" ? "124" : "123";
    const auto counters = mode == "counterfree" ? "allocations=0 requested_bytes=0" : "allocations=1 requested_bytes=8";
    std::cout << "seconds=" << seconds << " checksum=" << checksum << " " << counters << "\n";
    return 0;
}
