#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

namespace wio::wir_backend {
class NativeCallScope final {
    struct Lease { std::atomic<bool> active{true}; std::thread::id thread = std::this_thread::get_id(); };
    std::shared_ptr<Lease> lease_ = std::make_shared<Lease>();
public:
    ~NativeCallScope() { lease_->active = false; }
    template<class R, class... A>
    std::function<R(A...)> callback(std::function<R(A...)> fn, bool retained, bool anyThread) const {
        return [fn = std::move(fn), lease = lease_, retained, anyThread](A... args) -> R {
            if (!retained && !lease->active.load()) throw std::runtime_error("expired call-scoped Wio callback");
            if (!anyThread && lease->thread != std::this_thread::get_id()) throw std::runtime_error("Wio callback entered from the wrong thread");
            return fn(std::forward<A>(args)...);
        };
    }
};
}
