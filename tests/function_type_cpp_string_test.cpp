#include "wio/sema/type.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    using namespace wio::common;
    using namespace wio::sema;

    const Ref<Type> i32 = MakeRef<PrimitiveType>("i32");
    const Ref<Type> i64 = MakeRef<PrimitiveType>("i64");
    const Ref<Type> string = MakeRef<PrimitiveType>("string");
    const Ref<Type> callback = MakeRef<FunctionType>(
        std::vector<Ref<Type>>{i32, string},
        i64
    );

    const std::string expected = "std::function<int64_t(int32_t, wio::String)>";
    if (callback->toCppString() != expected)
    {
        std::cerr << "expected: " << expected << "\nactual: " << callback->toCppString() << '\n';
        return 1;
    }

    const Ref<Type> nested = MakeRef<FunctionType>(
        std::vector<Ref<Type>>{callback},
        MakeRef<PrimitiveType>("bool")
    );
    const std::string nestedExpected =
        "std::function<bool(std::function<int64_t(int32_t, wio::String)>)>";
    if (nested->toCppString() != nestedExpected)
    {
        std::cerr << "expected: " << nestedExpected << "\nactual: " << nested->toCppString() << '\n';
        return 1;
    }

    return 0;
}
