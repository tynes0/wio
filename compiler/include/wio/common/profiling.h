#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <coco.h>

namespace wio::common::profiling
{
    class Session
    {
    public:
        Session(std::string name, const std::filesystem::path& outputPath)
        {
            coco::instrumentor::get().begin_session(name, outputPath);
            active_ = coco::instrumentor::get().is_active();
        }

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        ~Session()
        {
            if (active_)
                coco::instrumentor::get().end_session();
        }

        [[nodiscard]] bool active() const noexcept { return active_; }

    private:
        bool active_ = false;
    };

    class Scope
    {
    public:
        explicit Scope(std::string_view name)
        {
            if (coco::instrumentor::get().is_active())
                timer_.emplace(std::string(name));
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;

        void stop()
        {
            if (timer_)
            {
                timer_->stop();
                timer_.reset();
            }
        }

    private:
        std::optional<coco::instrumentation_timer> timer_;
    };
}
