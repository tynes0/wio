#include <wio_native_sdk.h>
#include <limits>
#include <iostream>

int main(int argc, char** argv) {
    using namespace wio::sdk;
    if (argc != 2)
        return 1;
    try {
        auto module = NativeModule::open(argv[1]);
        module.start();
        auto app = module.application();
        if (app.name() != "Demo" || app.stages().size() < 3 ||
            !(module.legacyApi()->capabilities & WIO_MODULE_CAP_APPLICATION_SCHEDULE_V1))
            return 2;
        app.start();
        if (module.invoke("Trace").asString() != "app>input>sim>" || module.invoke("Initial").asInteger() != 41) {
            std::cerr << "start state: " << module.invoke("Trace").asString() << ' '
                      << module.invoke("Initial").asInteger() << '\n';
            return 3;
        }
        const auto* raw = module.legacyApi()->application;
        auto* storage = ::operator new(raw->stateSize, std::align_val_t(raw->stateAlignment));
        if (raw->construct(storage) != WIO_APPLICATION_OK)
            return 4;
        int foreign = 0;
        std::thread worker([&] { foreign = raw->start(storage); });
        worker.join();
        if (foreign != WIO_APPLICATION_WRONG_THREAD)
            return 5;
        raw->destroy(storage);
        ::operator delete(storage, std::align_val_t(raw->stateAlignment));
        bool rejected = false;
        try {
            (void)app.update(std::numeric_limits<double>::quiet_NaN());
        } catch (...) {
            rejected = true;
        }
        if (!rejected)
            return 6;
        (void)app.update(0.001);
        (void)app.update(0.001);
        (void)app.update(0.001);
        if (!app.exit_requested() || module.invoke("Ticks").asInteger() != 3)
            return 7;
        app.close();
        if (module.invoke("Trace").asString() != "app>input>sim>sim.close>input.close>app.close>")
            return 8;
        auto failure = NativeValue::boolean(true);
        module.invoke("FailStart", std::span(&failure, 1));
        auto rollback = module.application();
        rejected = false;
        try {
            rollback.start();
        } catch (...) {
            rejected = true;
        }
        if (!rejected || module.invoke("Trace").asString() != "app>input>input.close>app.close>")
            return 9;
        module.close(); // Application leases pin their module until host destruction.
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 99;
    }
}
