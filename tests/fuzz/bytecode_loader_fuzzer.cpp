#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size > 16u * 1024u * 1024u)
        return 0;
    const auto bytes = std::span{reinterpret_cast<const std::byte*>(data), size};
    wio::bytecode::DecodeLimits limits;
    limits.maximumFileBytes = 16u * 1024u * 1024u;
    limits.maximumStrings = 100'000;
    limits.maximumRecords = 100'000;
    limits.maximumListElements = 500'000;
    wio::bytecode::DecodeResult decoded = wio::bytecode::decode(bytes, limits);
    if (decoded.succeeded())
        (void)wio::bytecode::Verifier{}.verify(decoded.module);
    return 0;
}
