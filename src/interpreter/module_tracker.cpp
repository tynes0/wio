#include "module_tracker.h"

#include <set>
#include <filesystem>
#include "../utils/filesystem.h"

namespace wio
{
    static std::set<std::filesystem::path> s_modules;

    bool module_tracker::add_module(const std::filesystem::path& filepath)
    {
        auto abs_path = filesystem::get_abs_path(filepath);
        
        if(s_modules.contains(abs_path))
            return false;

        s_modules.insert(filepath);
        return true;
    }

    bool module_tracker::is_imported(const std::filesystem::path& filepath)
    {
        return s_modules.contains(filesystem::get_abs_path(filepath));
    }

    module_tracker& module_tracker::get()
    {
        static module_tracker s_tracker;
        return s_tracker;
    }
}
