/*
 * This file is part of the Coco library, originally created by Tynes0.
 * Coco is a small header-only timing/profiling utility.
 *
 * Features:
 * - Scope timers for console output
 * - Timer statistics
 * - Chrome/Edge compatible trace-event JSON generation
 *
 * Trace files can be opened from:
 * - chrome://tracing
 * - edge://tracing
 * - Perfetto UI
 *
 * Released under the MIT License.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sch = std::chrono;

#if !defined(COCO_ASSERT)
#   if defined(_DEBUG)
#define COCO_ASSERT(condition, message)                                                              \
    do                                                                                                \
    {                                                                                                 \
        if (!(condition))                                                                             \
        {                                                                                             \
            std::cerr << "Assertion failed: " << #condition << " (" << message << ")" << std::endl; \
            std::cerr << "File: " << __FILE__ << ", Line: " << __LINE__ << std::endl;                \
            std::abort();                                                                             \
        }                                                                                             \
    } while (false)
#   else
#define COCO_ASSERT(condition, message) ((void)0)
#   endif
#endif

namespace coco
{
    using clock_t = sch::steady_clock;

    namespace time_units
    {
        struct nanoseconds
        {
            using type = sch::nanoseconds;
            static constexpr const char* name = "nanoseconds";
        };

        struct microseconds
        {
            using type = sch::microseconds;
            static constexpr const char* name = "microseconds";
        };

        struct milliseconds
        {
            using type = sch::milliseconds;
            static constexpr const char* name = "milliseconds";
        };

        struct seconds
        {
            using type = sch::seconds;
            static constexpr const char* name = "seconds";
        };

        struct minutes
        {
            using type = sch::minutes;
            static constexpr const char* name = "minutes";
        };

        struct hours
        {
            using type = sch::hours;
            static constexpr const char* name = "hours";
        };
    }

    namespace detail
    {
        template <typename T>
        struct is_duration_unit : std::false_type {};

        template <>
        struct is_duration_unit<time_units::nanoseconds> : std::true_type {};

        template <>
        struct is_duration_unit<time_units::microseconds> : std::true_type {};

        template <>
        struct is_duration_unit<time_units::milliseconds> : std::true_type {};

        template <>
        struct is_duration_unit<time_units::seconds> : std::true_type {};

        template <>
        struct is_duration_unit<time_units::minutes> : std::true_type {};

        template <>
        struct is_duration_unit<time_units::hours> : std::true_type {};

        template <typename T>
        inline constexpr bool is_duration_unit_v = is_duration_unit<T>::value;

        inline int64_t time_point_to_microseconds(const clock_t::time_point& timepoint)
        {
            return sch::duration_cast<sch::microseconds>(timepoint.time_since_epoch()).count();
        }

        inline uint32_t current_thread_index()
        {
            static std::atomic<uint32_t> next_id{ 0 };
            thread_local uint32_t thread_id = next_id.fetch_add(1, std::memory_order_relaxed);
            return thread_id;
        }

        inline std::string json_escape(const std::string& input)
        {
            std::string output;
            output.reserve(input.size());

            for (unsigned char c : input)
            {
                switch (c)
                {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (c < 0x20)
                        output += '?';
                    else
                        output += static_cast<char>(c);
                    break;
                }
            }

            return output;
        }

        struct profile_result
        {
            std::string name;
            int64_t start_us = 0;
            int64_t end_us = 0;
            uint32_t thread_id = 0;
        };

        struct instrumentation_session
        {
            std::string name;
        };
    }

    template <typename From, typename To>
    long long duration_count_cast(long long value)
    {
        static_assert(detail::is_duration_unit_v<From>, "From must be a coco::time_units type.");
        static_assert(detail::is_duration_unit_v<To>, "To must be a coco::time_units type.");

        typename From::type from_duration(value);
        return sch::duration_cast<typename To::type>(from_duration).count();
    }

    class instrumentor
    {
    public:
        instrumentor(const instrumentor&) = delete;
        instrumentor& operator=(const instrumentor&) = delete;

        ~instrumentor()
        {
            end_session();
        }

        void begin_session(const std::string& name, const std::filesystem::path& filepath = "results.json")
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_active)
                internal_end_session();

            m_output_stream.open(filepath, std::ios::out | std::ios::trunc);
            if (!m_output_stream.is_open())
            {
                COCO_ASSERT(false, "Failed to open profiling output file.");
                return;
            }

            m_current_session = std::make_unique<detail::instrumentation_session>(detail::instrumentation_session{ name });
            m_session_start_us = detail::time_point_to_microseconds(clock_t::now());
            m_profile_count = 0;
            m_active = true;

            write_header();
        }

        void end_session()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_active)
                return;

            internal_end_session();
        }

        void write_profile(const detail::profile_result& result)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            COCO_ASSERT(m_active, "write_profile() called on inactive instrumentor.");
            if (!m_active || !m_output_stream.is_open())
                return;

            int64_t duration = result.end_us - result.start_us;
            int64_t timestamp = result.start_us - m_session_start_us;

            if (timestamp < 0)
            {
                duration += timestamp;
                timestamp = 0;
            }

            if (duration < 0)
                duration = 0;

            if (m_profile_count++ > 0)
                m_output_stream << ',';

            const std::string escaped_name = detail::json_escape(result.name);

            m_output_stream << "{\"cat\":\"function\",";
            m_output_stream << "\"dur\":" << duration << ',';
            m_output_stream << "\"name\":\"" << escaped_name << "\",";
            m_output_stream << "\"ph\":\"X\",";
            m_output_stream << "\"pid\":0,";
            m_output_stream << "\"tid\":" << result.thread_id << ',';
            m_output_stream << "\"ts\":" << timestamp;
            m_output_stream << '}';
        }

        bool is_active() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_active;
        }

        static instrumentor& get()
        {
            static instrumentor instance;
            return instance;
        }

    private:
        instrumentor() = default;

        void write_header()
        {
            m_output_stream << "{\"displayTimeUnit\":\"ms\",\"otherData\":{},\"traceEvents\":[";
        }

        void write_footer()
        {
            m_output_stream << "]}";
        }

        void internal_end_session()
        {
            write_footer();
            m_output_stream.flush();
            m_output_stream.close();

            m_current_session.reset();
            m_profile_count = 0;
            m_session_start_us = 0;
            m_active = false;
        }

    private:
        std::unique_ptr<detail::instrumentation_session> m_current_session;
        std::ofstream m_output_stream;
        int m_profile_count = 0;
        int64_t m_session_start_us = 0;
        bool m_active = false;
        mutable std::mutex m_mutex;
    };

    struct dont_start {};

    template <typename Duration = time_units::microseconds>
    class timer
    {
    public:
        static_assert(detail::is_duration_unit_v<Duration>, "Duration must be a coco::time_units type.");

        timer(const std::string& name = "Coco Timer", bool print_state = false)
            : m_name(name), m_print_when_stopped(print_state)
        {
            start();
        }

        explicit timer(dont_start, const std::string& name = "Coco Timer", bool print_state = false)
            : m_name(name), m_print_when_stopped(print_state)
        {
        }

        timer(const timer&) = delete;
        timer& operator=(const timer&) = delete;

        timer(timer&& other) noexcept
        {
            move_from(std::move(other));
        }

        timer& operator=(timer&& other) noexcept
        {
            if (this != &other)
                move_from(std::move(other));
            return *this;
        }

        ~timer()
        {
            stop();
        }

        void start()
        {
            m_time = 0;
            m_paused = false;
            m_stopped = false;
            m_timepoint = now();
        }

        void pause()
        {
            COCO_ASSERT(!m_stopped, "pause() called on inactive timer.");
            if (m_stopped || m_paused)
                return;

            m_time += elapsed_since_timepoint();
            m_paused = true;
        }

        void resume()
        {
            COCO_ASSERT(!m_stopped, "resume() called on inactive timer.");
            if (m_stopped || !m_paused)
                return;

            m_paused = false;
            m_timepoint = now();
        }

        void reset()
        {
            m_time = 0;
            m_paused = false;
            m_stopped = false;
            m_timepoint = now();
        }

        void stop()
        {
            if (m_stopped)
                return;

            if (!m_paused)
                m_time += elapsed_since_timepoint();

            m_stopped = true;
            m_paused = false;

            if (m_print_when_stopped)
                std::cout << m_name << " : " << m_time << ' ' << Duration::name << '\n';
        }

        bool completed_on_time(long long time) const noexcept
        {
            if (!m_stopped)
                return false;
            return m_time <= time;
        }

        bool complated_on_time(long long time) const noexcept
        {
            return completed_on_time(time);
        }

        bool is_running() const noexcept
        {
            return !m_stopped;
        }

        bool is_paused() const noexcept
        {
            return m_paused;
        }

        void set_print_state(bool state)
        {
            m_print_when_stopped = state;
        }

        bool is_printing() const noexcept
        {
            return m_print_when_stopped;
        }

        long long get_time() const
        {
            if (m_stopped || m_paused)
                return m_time;

            return m_time + elapsed_since_timepoint();
        }

        template <typename To>
        long long get_casted_time() const
        {
            return duration_count_cast<Duration, To>(get_time());
        }

    private:
        clock_t::time_point now() const
        {
            return clock_t::now();
        }

        long long elapsed_since_timepoint() const
        {
            return sch::duration_cast<typename Duration::type>(now() - m_timepoint).count();
        }

        void move_from(timer&& other) noexcept
        {
            m_timepoint = other.m_timepoint;
            m_name = std::move(other.m_name);
            m_print_when_stopped = other.m_print_when_stopped;
            m_time = other.m_time;
            m_stopped = other.m_stopped;
            m_paused = other.m_paused;

            other.m_time = 0;
            other.m_stopped = true;
            other.m_paused = false;
            other.m_print_when_stopped = false;
        }

    private:
        clock_t::time_point m_timepoint{};
        std::string m_name = "Coco Timer";
        bool m_print_when_stopped = false;
        long long m_time = 0;
        bool m_stopped = true;
        bool m_paused = false;
    };

    template <typename T>
    struct is_timer : std::false_type {};

    template <typename Duration>
    struct is_timer<timer<Duration>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_timer_v = is_timer<std::remove_cv_t<std::remove_reference_t<T>>>::value;

    class instrumentation_timer
    {
    public:
        explicit instrumentation_timer(const std::string& name)
            : m_name(name)
        {
            start();
        }

        instrumentation_timer(const std::string& name, dont_start)
            : m_name(name)
        {
        }

        instrumentation_timer(const instrumentation_timer&) = delete;
        instrumentation_timer& operator=(const instrumentation_timer&) = delete;

        instrumentation_timer(instrumentation_timer&& other) noexcept
        {
            move_from(std::move(other));
        }

        instrumentation_timer& operator=(instrumentation_timer&& other) noexcept
        {
            if (this != &other)
                move_from(std::move(other));
            return *this;
        }

        ~instrumentation_timer()
        {
            stop();
        }

        void start()
        {
            m_time = 0;
            m_stopped = false;
            m_timepoint = clock_t::now();
        }

        void stop()
        {
            if (m_stopped)
                return;

            const clock_t::time_point end_timepoint = clock_t::now();

            const int64_t start = detail::time_point_to_microseconds(m_timepoint);
            const int64_t end = detail::time_point_to_microseconds(end_timepoint);

            m_time = end - start;
            if (m_time < 0)
                m_time = 0;

            instrumentor::get().write_profile({ m_name, start, end, detail::current_thread_index() });
            m_stopped = true;
        }

        bool completed_on_time(long long time) const noexcept
        {
            if (!m_stopped)
                return false;
            return m_time <= time;
        }

        bool complated_on_time(long long time) const noexcept
        {
            return completed_on_time(time);
        }

        bool is_running() const noexcept
        {
            return !m_stopped;
        }

        long long get_time() const
        {
            if (m_stopped)
                return m_time;

            const int64_t current = detail::time_point_to_microseconds(clock_t::now());
            const int64_t start = detail::time_point_to_microseconds(m_timepoint);
            const int64_t elapsed = current - start;
            return elapsed > 0 ? elapsed : 0;
        }

        template <typename To>
        long long get_casted_time() const
        {
            return duration_count_cast<time_units::microseconds, To>(get_time());
        }

    private:
        void move_from(instrumentation_timer&& other) noexcept
        {
            m_timepoint = other.m_timepoint;
            m_name = std::move(other.m_name);
            m_time = other.m_time;
            m_stopped = other.m_stopped;

            other.m_time = 0;
            other.m_stopped = true;
        }

    private:
        clock_t::time_point m_timepoint{};
        std::string m_name;
        long long m_time = 0;
        bool m_stopped = true;
    };

    class timer_statistics
    {
    public:
        void add_measurement(long long time)
        {
            m_measurements.push_back(time);
        }

        void clear_measurements()
        {
            m_measurements.clear();
        }

        bool empty() const noexcept
        {
            return m_measurements.empty();
        }

        double calculate_average() const
        {
            if (m_measurements.empty())
            {
                COCO_ASSERT(false, "no measurements found");
                return 0.0;
            }

            const long long sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0LL);
            return static_cast<double>(sum) / static_cast<double>(m_measurements.size());
        }

        double calculate_variance() const
        {
            if (m_measurements.empty())
            {
                COCO_ASSERT(false, "no measurements found");
                return 0.0;
            }

            const double avg = calculate_average();
            double variance = 0.0;

            for (long long time : m_measurements)
            {
                const double diff = static_cast<double>(time) - avg;
                variance += diff * diff;
            }

            return variance / static_cast<double>(m_measurements.size());
        }

        double calculate_standard_deviation() const
        {
            return std::sqrt(calculate_variance());
        }

        double calculate_median() const
        {
            if (m_measurements.empty())
            {
                COCO_ASSERT(false, "no measurements found");
                return 0.0;
            }

            std::vector<long long> sorted_measurements = m_measurements;
            std::sort(sorted_measurements.begin(), sorted_measurements.end());

            const size_t n = sorted_measurements.size();
            if (n % 2 == 0)
            {
                return (static_cast<double>(sorted_measurements[n / 2 - 1]) +
                        static_cast<double>(sorted_measurements[n / 2])) /
                       2.0;
            }

            return static_cast<double>(sorted_measurements[n / 2]);
        }

        long long get_min_value() const
        {
            if (m_measurements.empty())
            {
                COCO_ASSERT(false, "no measurements found");
                return 0;
            }

            return *std::min_element(m_measurements.begin(), m_measurements.end());
        }

        long long get_max_value() const
        {
            if (m_measurements.empty())
            {
                COCO_ASSERT(false, "no measurements found");
                return 0;
            }

            return *std::max_element(m_measurements.begin(), m_measurements.end());
        }

        size_t get_measurement_count() const noexcept
        {
            return m_measurements.size();
        }

    private:
        std::vector<long long> m_measurements;
    };

    class timer_data_logger
    {
    public:
        timer_data_logger()
            : m_stats(std::make_unique<timer_statistics>())
        {
        }

        explicit timer_data_logger(const timer_statistics& stats)
            : m_stats(std::make_unique<timer_statistics>(stats))
        {
        }

        explicit timer_data_logger(timer_statistics&& stats) noexcept
            : m_stats(std::make_unique<timer_statistics>(std::move(stats)))
        {
        }

        timer_data_logger(const timer_data_logger& other)
            : m_stats(std::make_unique<timer_statistics>(*other.m_stats))
        {
        }

        timer_data_logger(timer_data_logger&&) noexcept = default;
        ~timer_data_logger() = default;

        timer_data_logger& operator=(const timer_data_logger& other)
        {
            if (this != &other)
                m_stats = std::make_unique<timer_statistics>(*other.m_stats);
            return *this;
        }

        timer_data_logger& operator=(timer_data_logger&&) noexcept = default;

        timer_statistics* get_statistics() const
        {
            return m_stats.get();
        }

        void add_measurement(long long time)
        {
            m_stats->add_measurement(time);
        }

        template <typename Duration = time_units::microseconds>
        void log_statistics(const std::filesystem::path& filepath)
        {
            static_assert(detail::is_duration_unit_v<Duration>, "Duration must be a coco::time_units type.");

            std::ofstream file(filepath, std::ios::out | std::ios::trunc);
            if (!file.is_open())
            {
                COCO_ASSERT(false, "Failed to open file for writing.");
                return;
            }

            file << "Statistics Summary:\n";
            file << "-------------------\n";
            file << "Number of attempts: " << m_stats->get_measurement_count() << " times\n";
            file << "Average Time: " << m_stats->calculate_average() << ' ' << Duration::name << "\n";
            file << "Variance: " << m_stats->calculate_variance() << ' ' << Duration::name << "\n";
            file << "Standard Deviation: " << m_stats->calculate_standard_deviation() << ' ' << Duration::name << "\n";
            file << "Median Time: " << m_stats->calculate_median() << ' ' << Duration::name << "\n";
            file << "Minimum Time: " << m_stats->get_min_value() << ' ' << Duration::name << "\n";
            file << "Maximum Time: " << m_stats->get_max_value() << ' ' << Duration::name << "\n";
            file << "-------------------\n";
        }

    private:
        std::unique_ptr<timer_statistics> m_stats;
    };

    template <typename Duration = time_units::microseconds>
    class multiple_timer_manager
    {
    public:
        static_assert(detail::is_duration_unit_v<Duration>, "Duration must be a coco::time_units type.");

        void add_and_start_timer(const std::string& timer_name)
        {
            if (m_timers.find(timer_name) != m_timers.end())
            {
                COCO_ASSERT(false, "Timer already exists!");
                return;
            }

            m_timers.emplace(timer_name, std::make_unique<coco::timer<Duration>>(timer_name));
        }

        void stop_timer(const std::string& timer_name)
        {
            auto it = m_timers.find(timer_name);
            if (it == m_timers.end())
            {
                COCO_ASSERT(false, "Timer not found!");
                return;
            }

            it->second->stop();
            m_data_logger.add_measurement(it->second->get_time());
        }

        void reset_timer(const std::string& timer_name)
        {
            auto* found_timer = get_timer(timer_name);
            if (found_timer)
                found_timer->reset();
        }

        void pause_timer(const std::string& timer_name)
        {
            auto* found_timer = get_timer(timer_name);
            if (found_timer)
                found_timer->pause();
        }

        void resume_timer(const std::string& timer_name)
        {
            auto* found_timer = get_timer(timer_name);
            if (found_timer)
                found_timer->resume();
        }

        void remove_timer(const std::string& timer_name)
        {
            auto it = m_timers.find(timer_name);
            if (it == m_timers.end())
            {
                COCO_ASSERT(false, "Timer not found!");
                return;
            }

            m_timers.erase(it);
        }

        void reset_all_timers()
        {
            for (auto& [name, timer_ptr] : m_timers)
                timer_ptr->reset();
        }

        void stop_all_timers()
        {
            for (auto& [name, timer_ptr] : m_timers)
                timer_ptr->stop();
        }

        coco::timer<Duration>* get_timer(const std::string& timer_name)
        {
            auto it = m_timers.find(timer_name);
            if (it == m_timers.end())
            {
                COCO_ASSERT(false, "Timer not found!");
                return nullptr;
            }

            return it->second.get();
        }

        const coco::timer<Duration>* get_timer(const std::string& timer_name) const
        {
            auto it = m_timers.find(timer_name);
            if (it == m_timers.end())
            {
                COCO_ASSERT(false, "Timer not found!");
                return nullptr;
            }

            return it->second.get();
        }

        void log_statistics(const std::filesystem::path& filepath)
        {
            m_data_logger.log_statistics<Duration>(filepath);
        }

        bool is_timer_running(const std::string& timer_name) const
        {
            const auto* found_timer = get_timer(timer_name);
            return found_timer ? found_timer->is_running() : false;
        }

        long long get_elapsed_time(const std::string& timer_name) const
        {
            const auto* found_timer = get_timer(timer_name);
            return found_timer ? found_timer->get_time() : 0;
        }

        void rename_timer(const std::string& old_name, const std::string& new_name)
        {
            if (m_timers.find(new_name) != m_timers.end())
            {
                COCO_ASSERT(false, "New timer name already exists!");
                return;
            }

            auto it = m_timers.find(old_name);
            if (it == m_timers.end())
            {
                COCO_ASSERT(false, "Timer not found!");
                return;
            }

            m_timers.emplace(new_name, std::move(it->second));
            m_timers.erase(it);
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<coco::timer<Duration>>> m_timers;
        coco::timer_data_logger m_data_logger;
    };

    class timer_controller
    {
    public:
        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void start_timer(Timer& timer)
        {
            timer.start();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void stop_timer(Timer& timer)
        {
            timer.stop();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void reset_timer(Timer& timer)
        {
            timer.reset();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void pause_timer(Timer& timer)
        {
            timer.pause();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void resume_timer(Timer& timer)
        {
            timer.resume();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static bool is_timer_running(const Timer& timer)
        {
            return timer.is_running();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static bool is_timer_paused(const Timer& timer)
        {
            return timer.is_paused();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static long long get_timer_time(const Timer& timer)
        {
            return timer.get_time();
        }

        template <typename Timer, std::enable_if_t<is_timer_v<Timer>, int> = 0>
        static void set_timer_print_state(Timer& timer, bool state)
        {
            timer.set_print_state(state);
        }
    };

    template <class FunT, class... Args>
    void measure(size_t test_count, const std::filesystem::path& filepath, FunT&& fun, Args&&... args)
    {
        timer<time_units::microseconds> ctimer(dont_start{});
        timer_data_logger measurement_stats;

        for (size_t i = 0; i < test_count; ++i)
        {
            ctimer.start();
            std::invoke(fun, args...);
            ctimer.stop();
            measurement_stats.add_measurement(ctimer.get_time());
        }

        measurement_stats.log_statistics<time_units::microseconds>(filepath);
    }
}

#define _COCO_CONCAT_H(x, y) x##y
#define _COCO_CONCAT(x, y) _COCO_CONCAT_H(x, y)
#define _COCO_ADD_COUNTER(x) _COCO_CONCAT(x, __COUNTER__)

#ifndef COCO_NO_PROFILE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__FUNCSIG__)
#define _COCO_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define _COCO_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define _COCO_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define _COCO_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define _COCO_FUNC_SIG __func__
#else
#define _COCO_FUNC_SIG "_COCO_FUNC_SIG unknown!"
#endif

#define COCO_PROFILE_BEGIN_SESSION(name, filepath) ::coco::instrumentor::get().begin_session(name, filepath)
#define COCO_PROFILE_END_SESSION() ::coco::instrumentor::get().end_session()
#define COCO_PROFILE_SCOPE(name) ::coco::instrumentation_timer _COCO_ADD_COUNTER(__coco_profile_timer_)(name)
#define COCO_PROFILE_FUNCTION() COCO_PROFILE_SCOPE(_COCO_FUNC_SIG)
#else
#define COCO_PROFILE_BEGIN_SESSION(name, filepath) ((void)0)
#define COCO_PROFILE_END_SESSION() ((void)0)
#define COCO_PROFILE_SCOPE(name) ((void)0)
#define COCO_PROFILE_FUNCTION() ((void)0)
#endif

#define COCO_SCOPE_TIMER() ::coco::timer<::coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_){ "Coco Timer", true }
#define COCO_SCOPE_TIMER_NAMED(name) ::coco::timer<::coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)(name, true)

#define COCO_BEGIN_TIMER_PRINTABLE(timer_name) ::coco::timer<::coco::time_units::microseconds> _COCO_CONCAT(__coco_time_var_, timer_name){ "Coco Timer", true }
#define COCO_BEGIN_TIMER(timer_name) ::coco::timer<::coco::time_units::microseconds> _COCO_CONCAT(__coco_time_var_, timer_name)
#define COCO_END_TIMER(timer_name) ((_COCO_CONCAT(__coco_time_var_, timer_name)).stop())
#define COCO_GET_TIMER_VALUE(timer_name) ((_COCO_CONCAT(__coco_time_var_, timer_name)).get_time())
