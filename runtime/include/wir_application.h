#pragma once
#include <module_api.h>
#include "std_async.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <new>
#include <thread>

namespace wio::wir_backend {
// Hooks are generated solely from the canonical ApplicationDescriptor.
template <class Hooks> struct ApplicationHost {
    using Value = typename Hooks::Value;
    struct State {
        Value value{};
        std::thread::id owner = std::this_thread::get_id();
        bool started = false;
        bool closed = false;
        bool faulted = false;
        char error[1024]{};
    };

    static bool owns(const State* s) noexcept {
        return s && s->owner == std::this_thread::get_id();
    }
    static void fail(State& s, const char* text) noexcept {
        s.faulted = true;
        std::snprintf(s.error, sizeof(s.error), "%s", text ? text : "application failure");
    }

    template <class F> static std::int32_t guarded(State& s, F&& body) noexcept {
        try {
            body();
            return Hooks::exited(s.value) ? WIO_APPLICATION_EXIT_REQUESTED : WIO_APPLICATION_OK;
        } catch (const std::exception& e) {
            fail(s, e.what());
        } catch (...) {
            fail(s, "unknown application failure");
        }
        return WIO_APPLICATION_FAULTED;
    }

    static std::int32_t construct(void* storage) noexcept {
        if (!storage || reinterpret_cast<std::uintptr_t>(storage) % alignof(State))
            return WIO_APPLICATION_INVALID_STATE;
        State* state = nullptr;
        try {
            state = ::new (storage) State{};
            Hooks::construct(state->value);
            return WIO_APPLICATION_OK;
        } catch (...) {
            if (state)
                state->~State();
            return WIO_APPLICATION_FAULTED;
        }
    }

    static std::int32_t start(void* storage) noexcept {
        auto* s = static_cast<State*>(storage);
        if (!s)
            return WIO_APPLICATION_INVALID_STATE;
        if (!owns(s))
            return WIO_APPLICATION_WRONG_THREAD;
        if (s->started || s->closed || s->faulted)
            return WIO_APPLICATION_INVALID_STATE;
        const auto status = guarded(*s, [&] {
            runtime::BindAsyncMainExecutor();
            Hooks::start(s->value);
            s->started = true;
            runtime::DrainAsyncMainExecutor();
        });
        if (status == WIO_APPLICATION_FAULTED) {
            // The lowered close body tracks successfully started systems.
            s->closed = true;
            try {
                Hooks::close(s->value);
                runtime::DrainAsyncMainExecutor();
            } catch (...) {}
        }
        return status;
    }

    static std::int32_t update(void* storage, double delta) noexcept {
        auto* s = static_cast<State*>(storage);
        if (!s)
            return WIO_APPLICATION_INVALID_STATE;
        if (!owns(s))
            return WIO_APPLICATION_WRONG_THREAD;
        if (!s->started || s->closed || !std::isfinite(delta) || delta < 0)
            return WIO_APPLICATION_INVALID_STATE;
        if (s->faulted)
            return WIO_APPLICATION_FAULTED;
        if (Hooks::exited(s->value))
            return WIO_APPLICATION_EXIT_REQUESTED;
        return guarded(*s, [&] {
            runtime::DrainAsyncMainExecutor();
            Hooks::update(s->value, delta);
            runtime::DrainAsyncMainExecutor();
        });
    }

    static std::int32_t exit(void* storage, std::int32_t code) noexcept {
        auto* s = static_cast<State*>(storage);
        if (!s)
            return WIO_APPLICATION_INVALID_STATE;
        if (!owns(s))
            return WIO_APPLICATION_WRONG_THREAD;
        if (s->closed)
            return WIO_APPLICATION_INVALID_STATE;
        return guarded(*s, [&] { Hooks::exit(s->value, code); });
    }

    static std::int32_t close(void* storage) noexcept {
        auto* s = static_cast<State*>(storage);
        if (!s)
            return WIO_APPLICATION_INVALID_STATE;
        if (!owns(s))
            return WIO_APPLICATION_WRONG_THREAD;
        if (s->closed)
            return WIO_APPLICATION_ALREADY_CLOSED;
        if (!s->started)
            return WIO_APPLICATION_INVALID_STATE;
        s->closed = true; // Mark before user code: throwing/reentrant close cannot run twice.
        guarded(*s, [&] {
            runtime::DrainAsyncMainExecutor();
            Hooks::close(s->value);
            runtime::DrainAsyncMainExecutor();
        });
        return s->faulted ? WIO_APPLICATION_FAULTED : WIO_APPLICATION_OK;
    }

    static void destroy(void* storage) noexcept {
        auto* s = static_cast<State*>(storage);
        if (!owns(s))
            return; // Raw hosts must destroy on the constructing thread.
        if (s->started && !s->closed)
            close(s);
        s->~State();
    }

    static WioApplicationDescriptor descriptor(const char* name, std::uint32_t count,
                                               const WioApplicationStageDescriptor* stages) {
        return {name,
                sizeof(State),
                alignof(State),
                WIO_APPLICATION_MAIN_THREAD_AFFINE | WIO_APPLICATION_HOST_OWNS_STORAGE |
                    WIO_APPLICATION_NON_BLOCKING_UPDATE,
                0,
                &construct,
                &start,
                &update,
                &exit,
                &close,
                &destroy,
                +[](const void* p) {
                    auto* s = static_cast<const State*>(p);
                    return owns(s) && Hooks::exited(s->value);
                },
                +[](const void* p) -> std::int32_t {
                    auto* s = static_cast<const State*>(p);
                    return owns(s) ? Hooks::code(s->value) : 1;
                },
                +[](void* p) -> std::uint64_t {
                    auto* s = static_cast<State*>(p);
                    if (!owns(s) || s->closed)
                        return 0;
                    try {
                        return runtime::DrainAsyncMainExecutor();
                    } catch (...) {
                        fail(*s, "main executor failure");
                        return 0;
                    }
                },
                +[](const void* p) -> const char* {
                    auto* s = static_cast<const State*>(p);
                    return owns(s) ? s->error : "invalid application owner";
                },
                count,
                0,
                stages};
    }

    static int run() {
        State state;
        if (guarded(state, [&] { Hooks::construct(state.value); }) == WIO_APPLICATION_FAULTED)
            return 1;
        auto status = start(&state);
        auto previous = std::chrono::steady_clock::now();
        while (status == WIO_APPLICATION_OK) {
            const auto now = std::chrono::steady_clock::now();
            const double delta = std::chrono::duration<double>(now - previous).count();
            previous = now;
            status = update(&state, delta);
            std::this_thread::yield();
        }
        if (state.started && !state.closed)
            close(&state);
        return state.faulted ? 1 : Hooks::code(state.value);
    }
};
} // namespace wio::wir_backend
