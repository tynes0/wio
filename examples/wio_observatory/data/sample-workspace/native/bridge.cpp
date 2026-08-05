#include <cstdint>

// HACK: this fixture intentionally contains a finding for the audit report.
std::uint64_t NativeIdentity(std::uint64_t value)
{
    return value;
}
