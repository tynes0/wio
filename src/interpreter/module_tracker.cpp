#include "module_tracker.h"

#include <set>
#include "../utils/filesystem.h"

namespace wio
{
    static std::set<std::string> s_modules;

    bool module_tracker::add_module(const std::filesystem::path& filepath)
    {
        auto abs_path = filesystem::get_abs_path(filepath).string();
        
        if(s_modules.contains(abs_path))
            return false;

        s_modules.insert(abs_path);
        return true;
    }

    bool module_tracker::add_module(const std::string& module)
    {
        if (s_modules.contains(module))
            return false;

        s_modules.insert(module);
        return true;
    }

    bool module_tracker::is_imported(const std::filesystem::path& filepath)
    {
        return s_modules.contains(filesystem::get_abs_path(filepath).string());
    }

    bool module_tracker::is_imported(const std::string& module)
    {
        return s_modules.contains(module);
    }

    module_tracker& module_tracker::get()
    {
        static module_tracker s_tracker;
        return s_tracker;
    }
}
