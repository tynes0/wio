/*
 * This file is part of the dtlog library, originally created by Tynes0.
 * For the latest version and updates, please visit the official dtlog GitHub repository:
 * https://github.com/tynes0/dtlog
 *
 * dtlog is a basic library for logging, providing fast and user-friendly use
 * It is released under the MIT License. See the LICENSE file in the root of the dtlog repository
 * or visit the above GitHub link for more details.
 *
 * For contributions, bug reports, or other inquiries, feel free to contact the author:
 * - GitHub: https://github.com/tynes0
 * - Email: cihanbilgihan@gmail.com
 */

#pragma once

#include <mutex>     // std::mutex and std::lock_guard
#include <string>    // std::string.
#include <sstream>   // std::ostringstream.
#include <iomanip>   // std::setw and std::setfill.
#include <stdexcept> // std::runtime_error.
#include <memory>    // std::shared_ptr, std::make_shared
#include <fstream>   // std::ofstream
#include <algorithm> // std::for_each
#include <vector>    // std::vector
#include <filesystem>// std::filesystem

#define DTLOG_NODISCARD [[nodiscard]]   // I guess every modern compiler supports nodiscard attribute
#define DTLOG_UNUSED(x) (void)(x)       // Just to suppress some warnings...

namespace dtlog
{
/**
     * @brief A high-performance utility class for formatting strings with positional arguments support.
     * Supports:
     * - Automatic indexing: "{} {}"
     * - Positional indexing: "{1} {0}"
     * - Formatting specifiers: "{0:.2f}", "{:04X}"
     */
    class formatter
    {
        struct arg_wrapper
        {
            const void* ptr;
            void (*format_func)(std::ostringstream&, const void*, std::string_view);
        };

        template <typename T>
        static void format_helper(std::ostringstream& oss, const void* ptr, std::string_view spec)
        {
            const T& val = *static_cast<const T*>(ptr);
            
            std::ios state(nullptr);
            state.copyfmt(oss);
            
            parse_and_apply_spec(oss, spec);
            
            oss << val;
            
            oss.copyfmt(state);
        }

    public:
        template <typename... Args>
        static std::string format(std::string_view fmt, Args&&... args)
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return std::string(fmt);
            }
            else
            {
                arg_wrapper wrappers[] =
                {
                    { &args, format_helper<std::remove_reference_t<Args>> }... 
                };
                
                size_t num_args = sizeof...(Args);
    
                std::ostringstream oss;
                size_t arg_counter = 0;
                size_t pos = 0;
    
                while (pos < fmt.size())
                {
                    if (fmt[pos] == '{')
                    {
                        if (pos + 1 < fmt.size() && fmt[pos + 1] == '{')
                        {
                            oss << '{'; pos += 2; continue;
                        }
    
                        size_t end_brace = fmt.find('}', pos);
                        if (end_brace == std::string_view::npos)
                        {
                            oss << fmt.substr(pos);
                            break;
                        }
    
                        std::string_view content = fmt.substr(pos + 1, end_brace - pos - 1);
                        
                        size_t index = 0;
                        bool explicit_index = false;
                        size_t spec_start = 0;
    
                        if (!content.empty() && std::isdigit(content[0]))
                        {
                            size_t k = 0;
                            size_t idx_val = 0;
                            while (k < content.size() && std::isdigit(content[k]))
                            {
                                idx_val = idx_val * 10 + (content[k] - '0');
                                k++;
                            }
                            
                            if (k == content.size() || content[k] == ':')
                            {
                                index = idx_val;
                                explicit_index = true;
                                spec_start = k;
                            }
                        }
    
                        if (!explicit_index)
                            index = arg_counter++;
    
                        std::string_view spec;
                        if (spec_start < content.size() && content[spec_start] == ':')
                            spec = content.substr(spec_start + 1);
    
                        if (index < num_args)
                            wrappers[index].format_func(oss, wrappers[index].ptr, spec);
                        
                        pos = end_brace + 1;
                    }
                    else if (fmt[pos] == '}')
                    {
                        if (pos + 1 < fmt.size() && fmt[pos + 1] == '}')
                        {
                            oss << '}'; pos += 2;
                            continue;
                        }
                        
                        oss << '}'; pos++;
                    }
                    else
                    {
                        oss << fmt[pos]; pos++;
                    }
                }
                return oss.str();
            }
        }

    private:
        static void parse_and_apply_spec(std::ostringstream& oss, std::string_view spec)
        {
            if (spec.empty())
                return;

            size_t i = 0;

            if (i < spec.size() && spec[i] == '0')
            {
                oss << std::setfill('0');
                i++;
            }
            
            if (i < spec.size() && std::isdigit(spec[i]))
            {
                int width = 0;
                while (i < spec.size() && std::isdigit(spec[i]))
                {
                    width = width * 10 + (spec[i] - '0');
                    i++;
                }
                oss << std::setw(width);
            }

            if (i < spec.size() && spec[i] == '.')
            {
                i++;
                int prec = 0;
                while (i < spec.size() && std::isdigit(spec[i]))
                {
                    prec = prec * 10 + (spec[i] - '0');
                    i++;
                }
                oss << std::fixed << std::setprecision(prec);
            }

            for (; i < spec.size(); ++i)
            {
                switch (spec[i])
                {
                    case 'x': oss << std::hex; break;
                    case 'X': oss << std::uppercase << std::hex; break;
                    case 'd': oss << std::dec; break;
                    case 'o': oss << std::oct; break;
                    case 'f': oss << std::fixed; break;
                    case 'e': oss << std::scientific; break;
                    case 'b': oss << std::boolalpha; break;
                    case 'u':
                    case 'U': oss << std::uppercase; break;
                    default: break;
                }
            }
        }
    };

    /**
    * @brief A utility class for formatting date and time strings.
    */
    class date_time_formatter
    {
    public:
        /**
        * @brief Gets the current local time in a thread-safe manner.
        * @param t Reference to a std::tm structure to be filled with current time.
        */
        static void get_current_time(std::tm& t)
        {
            std::time_t now = std::time(nullptr);
#if defined(_WIN32) || defined(_WIN64)
            if (::localtime_s(&t, &now) != 0)
            {
                // Fallback or silent fail, usually doesn't happen unless invalid time
                t = {}; 
            }
#else
            if (::localtime_r(&now, &t) == nullptr)
            {
                t = {};
            }
#endif
        }

        /**
         * @brief Gets the full name of the weekday.
         * @param t The time structure.
         * @return The full name of the weekday (e.g., "Monday").
         */
        DTLOG_NODISCARD static std::string full_weekday_name(const std::tm& t)
        {
            return weekdays(t.tm_wday);
        }

        /**
         * @brief Gets the full name of the month.
         * @param t The time structure.
         * @return The full name of the month (e.g., "January").
         */
        DTLOG_NODISCARD static std::string full_month_name(const std::tm& t)
        {
            return months(t.tm_mon);
        }

        /**
         * @brief Gets the last two digits of the year.
         * @param t The time structure.
         * @return The last two digits of the year as a string.
         */
        DTLOG_NODISCARD static std::string year_2_digits(const std::tm& t)
        {
            return std::to_string(t.tm_year % 100);
        }

        /**
         * @brief Gets the full four digits of the year.
         * @param t The time structure.
         * @return The full four digits of the year as a string.
         */
        DTLOG_NODISCARD static std::string year_4_digits(const std::tm& t)
        {
            return std::to_string(t.tm_year + 1900);
        }

        /**
         * @brief Gets the date and time representation in a specific format.
         * @param t The time structure.
         * @return The formatted date and time string.
         */
        DTLOG_NODISCARD static std::string date_time_representation(const std::tm& t)
        {
            std::ostringstream oss;
            oss << weekdays(t.tm_wday)
                << " "
                << months(t.tm_mon)
                << " "
                << t.tm_mday
                << " "
                << t.tm_year + 1900
                << " "
                << format_time(t.tm_hour)
                << ":"
                << format_time(t.tm_min)
                << ":"
                << format_time(t.tm_sec);
            return oss.str();
        }

        /**
         * @brief Gets the short date representation in MM/DD/YY format.
         * @param t The time structure.
         * @return The short date string.
         */
        DTLOG_NODISCARD static std::string short_MMDDYY_date(const std::tm& t)
        {
            std::ostringstream oss;
            oss << format_time(t.tm_mon + 1)
                << "/"
                << format_time(t.tm_mday)
                << "/"
                << format_time((t.tm_year % 100));
            return oss.str();
        }

        /**
         * @brief Gets the month as a numeric string.
         * @param t The time structure.
         * @return The numeric representation of the month (1-12).
         */
        DTLOG_NODISCARD static std::string month(const std::tm& t)
        {
            return std::to_string(t.tm_mon + 1);
        }

        /**
         * @brief Gets the day of the month as a string.
         * @param t The time structure.
         * @return The day of the month.
         */
        DTLOG_NODISCARD static std::string day_of_month(const std::tm& t)
        {
            return std::to_string(t.tm_mday);
        }

        /**
         * @brief Gets the hours in 24-hour format.
         * @param t The time structure.
         * @return The hours in 24-hour format (0-23).
         */
        DTLOG_NODISCARD static std::string hours_24_format(const std::tm& t)
        {
            return std::to_string(t.tm_hour);
        }

        /**
         * @brief Gets the hours in 12-hour format.
         * @param t The time structure.
         * @return The hours in 12-hour format (1-12).
         */
        DTLOG_NODISCARD static std::string hours_12_format(const std::tm& t)
        {
            int hours12 = t.tm_hour % 12;
            if (hours12 == 0)
                hours12 = 12;
            return std::to_string(hours12);
        }

        /**
         * @brief Gets the minutes as a string.
         * @param t The time structure.
         * @return The minutes (0-59).
         */
        DTLOG_NODISCARD static std::string minutes(const std::tm& t)
        {
            return std::to_string(t.tm_min);
        }

        /**
         * @brief Gets the seconds as a string.
         * @param t The time structure.
         * @return The seconds (0-59).
         */
        DTLOG_NODISCARD static std::string seconds(const std::tm& t)
        {
            return std::to_string(t.tm_sec);
        }

        /**
         * @brief Gets the AM/PM designation.
         * @param t The time structure.
         * @return "AM" if the time is before noon, "PM" otherwise.
         */
        DTLOG_NODISCARD static std::string AM_PM(const std::tm& t)
        {
            return (t.tm_hour < 12) ? "AM" : "PM";
        }

        /**
         * @brief Gets the time in 12-hour clock format.
         * @param t The time structure.
         * @return The time in 12-hour format along with AM/PM designation.
         */
        DTLOG_NODISCARD static std::string clock_12_hour(const std::tm& t)
        {
            std::ostringstream oss;
            oss << format_time((t.tm_hour % 12 == 0) ? 12 : t.tm_hour % 12)
                << ":"
                << format_time(t.tm_min)
                << ":"
                << format_time(t.tm_sec) << " "
                << ((t.tm_hour < 12) ? "AM" : "PM");
            return oss.str();
        }

        /**
         * @brief Gets the time in HH:MM format (24-hour clock).
         * @param t The time structure.
         * @return The time in HH:MM format.
         */
        DTLOG_NODISCARD static std::string HHMM_time_24_hour(const std::tm& t)
        {
            std::ostringstream oss;
            oss << format_time(t.tm_hour)
                << ":"
                << format_time(t.tm_min);
            return oss.str();
        }

        /**
         * @brief Gets the time in ISO 8601 format (HH:MM:SS).
         * @param t The time structure.
         * @return The time in ISO 8601 format.
         */
        DTLOG_NODISCARD static std::string ISO8601_time_format(const std::tm& t)
        {
            std::ostringstream oss;
            oss << format_time(t.tm_hour)
                << ":"
                << format_time(t.tm_min)
                << ":"
                << format_time(t.tm_sec);
            return oss.str();
        }

    private:
        /**
         * @brief Formats the given time value with leading zeros if necessary.
         * @param time_value The time value to format.
         * @return The formatted time string (e.g., "05").
         */
        DTLOG_NODISCARD static std::string format_time(int time_value)
        {
            std::ostringstream oss;
            oss << std::setw(2)
                << std::setfill('0')
                << time_value;
            return oss.str();
        }

        /**
         * @brief Gets the full name of the weekday based on the day of the week.
         * @param wday The day of the week (0-6, Sunday-Saturday).
         * @return The full name of the weekday.
         */
        DTLOG_NODISCARD static std::string weekdays(int wday)
        {
            static const char* days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
            if (wday >= 0 && wday < 7) return days[wday];
            return "Invalid Day";
        }

        /**
         * @brief Gets the full name of the month based on the month index.
         * @param mon The month index (0-11, January-December).
         * @return The full name of the month.
         */
        DTLOG_NODISCARD static std::string months(int mon)
        {
            static const char* months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
            if (mon >= 0 && mon < 12) return months[mon];
            return "Invalid Month";
        }
    };

    /**
     * @brief Enumeration for different log levels.
     */
    enum class log_level : uint8_t
    {
        none,
        trace,
        info,
        debug,
        warning,
        error,
        critical
    };

    /**
     * @brief Converts a log level enum to its corresponding string representation.
     * @param level The log level enum.
     * @return The string representation of the log level.
     */
    DTLOG_NODISCARD inline std::string log_level_to_string(log_level level)
    {
        switch (level)
        {
        case dtlog::log_level::trace: return "trace";
        case dtlog::log_level::info: return "info";
        case dtlog::log_level::debug: return "debug";
        case dtlog::log_level::warning: return "warning";
        case dtlog::log_level::error: return "error";
        case dtlog::log_level::critical: return "critical";
        case dtlog::log_level::none: break;
        }
        return "none";
    }

    /**
     * @brief Abstract base interface for log sinks.
     */
    class isink
    {
    public:
        /**
         * @brief Default constructor.
         */
        isink() = default;

        /**
         * @brief Virtual destructor.
         */
        virtual ~isink() = default;

        /**
         * @brief Sets the minimum log level required to trigger an immediate flush.
         * @param level The log level threshold.
         */
        void flush_on(log_level level)
        {
            m_flush_level = level;
        }
        
        /**
         * @brief Logs a formatted message.
         * @param final_message The fully formatted log message to be written.
         * @param level The severity level of the log message.
         */
        virtual void log(const std::string& final_message, log_level level) = 0;

    protected:
        /**
         * @brief Checks if the buffer should be flushed based on the current message level.
         */
        bool should_flush(log_level level) const
        {
            return level >= m_flush_level;
        }
        
        std::mutex m_mutex; ///< Mutex for thread safety.
        log_level m_flush_level = log_level::trace; // Default: trace
    };

    /**
     * @brief A sink that writes log messages to the standard output (stdout).
     */
    class console_sink : public isink
    {
    public:
        /**
         * @brief Logs a message to the console.
         * @param msg The log message.
         * @param level The log level (used for coloring).
         */
        void log(const std::string& msg, log_level level) override;
    };

    /**
     * @brief A sink that writes log messages to the standard error (stderr).
     */
    class err_console_sink : public isink
    {
    public:
        /**
         * @brief Logs a message to stderr.
         * @param msg The log message.
         * @param level The log level (used for coloring).
         */
        void log(const std::string& msg, log_level level) override;
    };

    /**
    * @brief A sink that writes log messages to the debugger.
    */
    class debug_sink : public isink
    {
    public:
        /**
         * @brief Logs a message to debugger.
         * @param msg The log message.
         * @param level The log level.
         */
        void log(const std::string& msg, log_level level) override;
    };

    /**
     * @brief A sink that writes log messages to a file.
     */
    class file_sink : public isink
    {
    public:
        /**
         * @brief Constructs a file sink.
         * @param filename The name of the file to log to.
         * @throws std::runtime_error if the file cannot be opened.
         */
        explicit file_sink(const std::string& filename)
        {
            m_file.open(filename, std::ios::out | std::ios::app);
            if (!m_file.is_open()) {
                throw std::runtime_error("dtlog: Failed to open log file: " + filename);
            }
        }

        /**
         * @brief Destructor closes the file if it is open.
         */
        ~file_sink() override
        {
            if (m_file.is_open())
            {
                m_file.close();
            }
        }

        /**
         * @brief Logs a message to the file.
         * @param msg The log message.
         * @param level The log level.
         */
        void log(const std::string& msg, log_level level) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_file << msg;
            
            if (should_flush(level))
            {
                m_file.flush();
            }
        }

    private:
        std::ofstream m_file; ///< The file stream.
    };

    /**
     * @brief A sink that writes to a file and rotates it based on size.
     */
    class rotating_file_sink : public isink
    {
    public:
        /**
         * @brief Constructs a rotating file sink.
         * @param base_filename The base name of the log file.
         * @param max_size The maximum size of a log file in bytes before rotation.
         * @param max_files The maximum number of backup files to keep.
         */
        rotating_file_sink(std::string base_filename, size_t max_size, size_t max_files)
            : m_base_filename(std::move(base_filename)), m_max_size(max_size), m_max_files(max_files)
        {
            open_file();
        }

        /**
         * @brief Destructor closes the file if it is open.
         */
        ~rotating_file_sink() override
        {
            if (m_file.is_open())
                m_file.close();
        }

        /**
         * @brief Logs a message to the file, performing rotation if necessary.
         * @param msg The log message.
         * @param level The log level.
         */
        void log(const std::string& msg, log_level level) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            if (m_current_size + msg.size() > m_max_size)
            {
                rotate();
            }

            m_file << msg;
            m_current_size += msg.size();
            
            if (should_flush(level))
            {
                m_file.flush();
            }
        }

    private:
        /**
         * @brief Opens the log file in append mode.
         * @throws std::runtime_error if the file cannot be opened.
         */
        void open_file()
        {
            std::filesystem::path path(m_base_filename);
            std::filesystem::path parent_path = path.parent_path();

            if (!parent_path.empty() && !std::filesystem::exists(parent_path))
            {
                std::filesystem::create_directories(parent_path);
            }
            
            m_file.open(m_base_filename, std::ios::out | std::ios::app);
            
            if (!m_file.is_open())
                throw std::runtime_error("Cannot open log file");
            
            m_file.seekp(0, std::ios::end);
            m_current_size = static_cast<size_t>(m_file.tellp());
        }

        /**
         * @brief Rotates the log files when the size limit is reached.
         */
        void rotate()
        {
            namespace fs = std::filesystem;
            
            m_file.close();

            fs::path src_path(m_base_filename);
            std::string stem = src_path.stem().string();       // "app"
            std::string ext = src_path.extension().string();   // ".log"
            fs::path parent = src_path.parent_path();

            fs::path oldest_file = parent / (stem + "." + std::to_string(m_max_files) + ext);
            
            if (fs::exists(oldest_file))
            {
                fs::remove(oldest_file);
            }

            for (size_t i = m_max_files - 1; i >= 1; --i)
            {
                fs::path old_name = parent / (stem + "." + std::to_string(i) += ext);
                fs::path new_name = parent / (stem + "." + std::to_string(i + 1) += ext);
                
                if (fs::exists(old_name))
                {
                    fs::rename(old_name, new_name);
                }
            }

            fs::path first_backup = parent / (stem + ".1" + ext);
            if (fs::exists(src_path))
            {
                fs::rename(src_path, first_backup);
            }

            open_file();
        }

        std::string m_base_filename; ///< Base name of the log file.
        size_t m_max_size;           ///< Maximum size of the file in bytes.
        size_t m_max_files;          ///< Maximum number of backup files.
        size_t m_current_size = 0;   ///< Current size of the open file.
        std::ofstream m_file;        ///< The file stream.
    };

    /**
     * @brief A class for logging messages with various log levels and formatting options.
     * @tparam DefaultSink The type of the sink to be added by default (default: console_sink).
     */
    template <typename DefaultSink = console_sink, std::enable_if_t<std::is_base_of_v<isink, DefaultSink>>* = nullptr>
    class logger
    {
    public:
        /**
         * @brief Constructor for the logger.
         * Allows forwarding arguments to the DefaultSink constructor.
         * * @tparam SinkArgs Types of arguments for the DefaultSink constructor.
         * @param name The name of the logger.
         * @param pattern The log message pattern.
         * @param args Arguments to be passed to the DefaultSink constructor.
         */
        template <typename... SinkArgs>
        explicit logger(std::string name, std::string pattern = "[%R] %N: %V%n", SinkArgs&&... args)
            : log_name(std::move(name)), log_pattern(std::move(pattern))
        {
            m_sinks.push_back(std::make_shared<DefaultSink>(std::forward<SinkArgs>(args)...));
        }
        
        explicit logger(std::string name) 
            : log_name(std::move(name)), log_pattern("[%R] %N: %V%n")
        {
            m_sinks.push_back(std::make_shared<DefaultSink>());
        }
        logger(const logger&) = default;
        logger& operator=(const logger&) = default;
        logger(logger&&) = default;
        logger& operator=(logger&&) = default;

        /**
         * @brief Virtual destructor.
         */
        virtual ~logger() = default;

        /**
         * @brief Adds a new sink to the logger.
         * @param sink A shared pointer to the sink to be added.
         */
        void add_sink(const std::shared_ptr<isink>& sink)
        {
            m_sinks.push_back(sink);
        }

        /**
         * @brief Logs a message with the specified log level.
         * @tparam Args Variadic template for message arguments.
         * @param level The log level.
         * @param message The log message format string.
         * @param args Additional arguments for formatting the message.
         */
        template <class ...Args>
        void log(log_level level, const std::string& message, Args&&... args)
        {
            std::string user_message = formatter::format(message, std::forward<Args>(args)...);

            std::string final_log_message;
            pattern(level, user_message, final_log_message);

            for (auto& sink : m_sinks)
            {
                sink->log(final_log_message, level);
            }
        }

        /**
         * @brief Sets the name of the logger.
         * @param name The new name for the logger.
         */
        void set_name(const std::string& name)
        {
            log_name = name;
        }

        /**
         * @brief Gets the name of the logger.
         * @return The name of the logger.
         */
        DTLOG_NODISCARD std::string get_name() const
        {
            return log_name;
        }

        /**
         * @brief Sets the log message pattern.
         * @param pattern The new log message pattern.
         */
        void set_pattern(const std::string& pattern)
        {
            log_pattern = pattern;
        }

        /**
         * @brief Gets the log message pattern.
         * @return The current log message pattern.
         */
        std::string get_pattern() const
        {
            return log_pattern;
        }

        /**
         * @brief Sets the minimum log level required to trigger an immediate flush on all sinks.
         * @param level The log level threshold.
         */
        void flush_on(log_level level) const
        {
            for(auto& sink : m_sinks)
            {
                sink->flush_on(level);
            }
        }
        
        /**
        * @brief Logs a trace message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void trace(const std::string& message, Args&&... args)
        {
            return this->log(log_level::trace, message, std::forward<Args>(args)...);
        }

        /**
        * @brief Logs an info message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void info(const std::string& message, Args&&... args)
        {
            return this->log(log_level::info, message, std::forward<Args>(args)...);
        }

        /**
        * @brief Logs a debug message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void debug(const std::string& message, Args&&... args)
        {
            return this->log(log_level::debug, message, std::forward<Args>(args)...);
        }

        /**
        * @brief Logs a warning message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void warning(const std::string& message, Args&&... args)
        {
            return this->log(log_level::warning, message, std::forward<Args>(args)...);
        }

        /**
        * @brief Logs an error message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void error(const std::string& message, Args&&... args)
        {
            return this->log(log_level::error, message, std::forward<Args>(args)...);
        }

        /**
        * @brief Logs a critical message.
        * @tparam Args Variadic template for message arguments.
        * @param message The log message.
        * @param args Additional arguments for formatting the message.
        */
        template <class ...Args>
        void critical(const std::string& message, Args&&... args)
        {
            return this->log(log_level::critical, message, std::forward<Args>(args)...);
        }

    private:
        /**
         * @brief Formats the final log message based on the logger's pattern.
         * @param level The log level.
         * @param message The user-provided log message.
         * @param final_message Reference to the string where the formatted message will be stored.
         */
        void pattern(log_level level, const std::string& message, std::string& final_message) const
        {
            std::tm t;
            date_time_formatter::get_current_time(t);

            std::ostringstream oss;
            for (size_t i = 0; i < log_pattern.length(); ++i)
            {
                if (log_pattern[i] != '%')
                {
                    oss << log_pattern[i];
                    continue;
                }

                if (i + 1 >= log_pattern.length())
                {
                    oss << '%';
                    break;
                }

                switch (log_pattern[i + 1])
                {
                case 'V': oss << message; break;
                case 'N': oss << log_name; break;
                case 'L': oss << log_level_to_string(level); break;
                case 'A': oss << date_time_formatter::full_weekday_name(t); break;
                case 'B': oss << date_time_formatter::full_month_name(t); break;
                case 'C': oss << date_time_formatter::year_2_digits(t); break;
                case 'Y': oss << date_time_formatter::year_4_digits(t); break;
                case 'R': oss << date_time_formatter::date_time_representation(t); break;
                case 'D': oss << date_time_formatter::short_MMDDYY_date(t); break;
                case 'm': oss << date_time_formatter::month(t); break;
                case 'd': oss << date_time_formatter::day_of_month(t); break;
                case 'H': oss << date_time_formatter::hours_24_format(t); break;
                case 'h': oss << date_time_formatter::hours_12_format(t); break;
                case 'M': oss << date_time_formatter::minutes(t); break;
                case 'S': oss << date_time_formatter::seconds(t); break;
                case 'F': oss << date_time_formatter::AM_PM(t); break;
                case 'x': oss << date_time_formatter::clock_12_hour(t); break;
                case 'X': oss << date_time_formatter::HHMM_time_24_hour(t); break;
                case 'T': oss << date_time_formatter::ISO8601_time_format(t); break;
                case '%': oss << '%'; break;
                case 'n': oss << '\n'; break;
                default: 
                    oss << '%' << log_pattern[i + 1];
                    break;
                }
                
                i++; 
            }
            
            final_message = oss.str();
        }

        std::string log_name;                        ///< The name of the logger.
        std::string log_pattern;                     ///< The log message pattern.
        std::vector<std::shared_ptr<isink>> m_sinks; ///< The list of sinks.
    };
} // namespace dtlog