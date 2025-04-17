#include "module_tracker.h"

#include <set>
#include "../utils/filesystem.h"

namespace wio
{
    static std::set<std::string> s_modules;

    id_t module_tracker::add_module(const std::filesystem::path& filepath)
    {
        auto abs_path = filesystem::get_abs_path(filepath).string();
        
        if(s_modules.contains(abs_path))
            return 0;

        s_modules.insert(abs_path);
        return hash_path_to_id(abs_path);
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

    id_t module_tracker::hash_path_to_id(const std::string& path)
    {
        id_t hash = 2166136261u;
        for (char c : path) 
        {
            hash ^= static_cast<uint8_t>(tolower(c));
            hash *= 16777619u;
        }
        return hash;
    }
}
