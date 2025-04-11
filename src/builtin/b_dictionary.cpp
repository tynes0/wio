#include "b_dictionary.h"

#include "builtin_base.h"

#include "../variables/dictionary.h"

namespace wio
{
    namespace detail
    {
        static ref<var_dictionary> get_var_str(ref<variable_base> base_dictionary)
        {
            if (!base_dictionary)
                throw exception("Dictionary is null!");
            ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base_dictionary);
            if (!dict)
                throw exception("Dictionary is null!");

            return dict;
        }
    }

    namespace builtin
    {
        ref<scope> b_dictionary::load()
        {
            auto result = make_ref<scope>(scope_type::builtin);


            return result;
        }
    }
}
