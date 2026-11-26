#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <wir_abi.h>

namespace wio::wir_backend
{
    template <class T> void rejectCallbackCapture(const T& value)
    {
        if constexpr (requires { value.rejectCallbackCapture(); })
            value.rejectCallbackCapture();
    }
    template <class T> T callbackBorrowArgument(T value)
    {
        if constexpr (requires { value.withCallbackLifetime(); })
            return value.withCallbackLifetime();
        else
            return value;
    }
    template <class R, class... A> R callbackBorrowResult(R result, const std::tuple<A...>& arguments)
    {
        if constexpr (requires {
                          result.borrowIdentity();
                          result.canRebind();
                          result.asView();
                      })
        {
            bool matched = false;
            R caller;
            std::apply(
                [&](const auto&... argument)
                {
                    (
                        [&]
                        {
                            if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, R>)
                            {
                                if (!matched && argument.borrowIdentity() == result.borrowIdentity() &&
                                    (!result.canRebind() || argument.canRebind()))
                                {
                                    caller = result.canRebind() ? argument : argument.asView();
                                    matched = true;
                                }
                            }
                        }(),
                        ...);
                },
                arguments);
            if (!matched)
                throw std::runtime_error("callback result must alias an explicit compatible caller borrow");
            return caller;
        }
        else
        {
            return bindOwningNativeResult(std::move(result));
        }
    }
    class NativeCallScope final
    {
        struct Lease
        {
            std::atomic<bool> active{true};
            std::thread::id thread = std::this_thread::get_id();
        };
        std::shared_ptr<Lease> lease_ = std::make_shared<Lease>();

    public:
        ~NativeCallScope()
        {
            lease_->active = false;
        }
        template <class R, class... A>
        std::function<R(A...)> callback(std::function<R(A...)> fn, bool retained, bool anyThread) const
        {
            bindOwningNativeCallable(fn);
            return [fn = std::move(fn), lease = lease_, retained, anyThread](A... args) -> R
            {
                if (!retained && !lease->active.load())
                    throw std::runtime_error("expired call-scoped Wio callback");
                if (!anyThread && lease->thread != std::this_thread::get_id())
                    throw std::runtime_error("Wio callback entered from the wrong thread");
                const NativeCallableInvocationScope invocation{true};
                CallbackBorrowCells<sizeof...(A)> cells;
                (cells.add(args), ...);
                constexpr bool borrowed = (requires(A value) { value.withCallbackLifetime(); } || ...);
                abi::Frame frame{borrowed};
                if constexpr (std::is_void_v<R>)
                    fn(callbackBorrowArgument(std::forward<A>(args))...);
                else
                {
                    const auto caller = std::tuple<A...>{args...};
                    return callbackBorrowResult(fn(callbackBorrowArgument(std::forward<A>(args))...), caller);
                }
            };
        }
    };
} // namespace wio::wir_backend
