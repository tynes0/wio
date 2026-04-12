#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <array>
#include <format>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include <exception.h>
#include <fit.h>
#include <module_api.h>
#include <ref.h>

#if defined(_WIN32)
#define WIO_EXPORT __declspec(dllexport)
#else
#define WIO_EXPORT __attribute__((visibility("default")))
#endif

namespace wio
{
    using String = std::string;
    using WString = std::wstring;
}

namespace wio
{
    template <typename T>
    using DArray = std::vector<T>;
    
    template <typename T, size_t N>
    using SArray = std::array<T, N>;
    
    template <typename K, typename V>
    using Dict = std::unordered_map<K, V>;
    
    template <typename K, typename V>
    using Tree = std::map<K, V>;
}

#include "io.h"
struct _WS_alpha_Counter;
struct _WS_beta_Counter;
struct _WS_alpha_Counter : public wio::runtime::RefCountedObject{
    static constexpr uint64_t TYPE_ID = 237226537168666798ull;
    virtual uint64_t _WF_GetTypeID() const { return 237226537168666798ull; }
    virtual bool _WF_IsA(uint64_t id) const override {
        if (id == 237226537168666798ull) return true;
        return false;
    }

    virtual void* _WF_CastTo(uint64_t id) override {
        if (id == 237226537168666798ull) return this;
        return nullptr;
    }

    friend class wio::runtime::Ref<_WS_alpha_Counter>;
    int32_t value;
    _WS_alpha_Counter() = default;
    _WS_alpha_Counter(const _WS_alpha_Counter&) = default;
    _WS_alpha_Counter(int32_t _value) : value(_value) {}
};

struct _WS_beta_Counter : public wio::runtime::RefCountedObject{
    static constexpr uint64_t TYPE_ID = 17445833154805873026ull;
    virtual uint64_t _WF_GetTypeID() const { return 17445833154805873026ull; }
    virtual bool _WF_IsA(uint64_t id) const override {
        if (id == 17445833154805873026ull) return true;
        return false;
    }

    virtual void* _WF_CastTo(uint64_t id) override {
        if (id == 17445833154805873026ull) return this;
        return nullptr;
    }

    friend class wio::runtime::Ref<_WS_beta_Counter>;
    int32_t value;
    _WS_beta_Counter() = default;
    _WS_beta_Counter(const _WS_beta_Counter&) = default;
    _WS_beta_Counter(int32_t _value) : value(_value) {}
};


wio::runtime::Ref<_WS_alpha_Counter> _WF_alpha_Make();


wio::runtime::Ref<_WS_beta_Counter> _WF_beta_Make();


wio::runtime::Ref<_WS_alpha_Counter> _WF_alpha_Make()
{
    return wio::runtime::Ref<_WS_alpha_Counter>::Create(1);
}

wio::runtime::Ref<_WS_beta_Counter> _WF_beta_Make()
{
    return wio::runtime::Ref<_WS_beta_Counter>::Create(2);
}

int main(int argc, char** argv) {
    wio::DArray<wio::String> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(wio::String(argv[i]));
    }
    
    try {
        wio::runtime::Ref<_WS_alpha_Counter> left = _WF_alpha_Make();
        wio::runtime::Ref<_WS_beta_Counter> right = _WF_beta_Make();
        wio::runtime::io::bPrint(std::format("Realm counters: {}", (left->value + right->value)));
        return 0;
    }
    catch (const wio::runtime::RuntimeException& ex)
    {
        std::cout << "Runtime Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
