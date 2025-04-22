#include "math.h"
#include <cmath>
#include <stdexcept>
#include <sstream>

#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../variables/function.h"

#include "../base/exception.h"

#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"

#include "../utils/util.h"
#include "../utils/rand_manager.h"

#include "../interpreter/main_table.h"

#include "builtin_base.h"
#include "helpers.h"

namespace wio 
{
    namespace builtin 
    {
        namespace detail 
        {
            static rand_manager& get_rman()
            {
                static rand_manager s_rand_manager;
                return s_rand_manager;
            }

            static ref<variable_base> b_abs(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::abs(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_sin(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::sin(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_cos(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::cos(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_tan(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::tan(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_sqrt(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                if (val < 0.0)
                    throw builtin_error("Cannot take square root of negative number");

                return make_ref<variable>(any(std::sqrt(val)), variable_type::vt_float);
            }

            static ref<variable_base> b_pow(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::pow(helper::var_as_double(args[0]), helper::var_as_double(args[1]))), variable_type::vt_float);
            }

            static ref<variable_base> b_log(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                 if (val <= 0.0)
                     throw builtin_error("Logarithm of zero or negative number is undefined.");

                 return make_ref<variable>(any(std::log(val)), variable_type::vt_float);
                
            }

            static ref<variable_base> b_log10(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                if (val <= 0.0)
                    throw builtin_error("Logarithm of zero or negative number is undefined.");

                return make_ref<variable>(any(std::log10(val)), variable_type::vt_float);
            }

            static ref<variable_base> b_asin(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                if (val < -1.0 || val > 1.0) 
                    throw builtin_error("ArcSin input must be in the range [-1, 1]");

                return make_ref<variable>(any(std::asin(val)), variable_type::vt_float);             
            }

            static ref<variable_base> b_acos(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                if (val < -1.0 || val > 1.0)
                    throw builtin_error("ArcCos input must be in the range [-1, 1]");

                return make_ref<variable>(any(std::acos(val)), variable_type::vt_float);
            }

            static ref<variable_base> b_atan(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::atan(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_ceil(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::ceil(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_floor(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::floor(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_round(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::round(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_exp(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::exp(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_atan2(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::atan2(helper::var_as_double(args[0]), helper::var_as_double(args[1]))), variable_type::vt_float);
            }

            static ref<variable_base> b_fmod(const std::vector<ref<variable_base>>& args)
            {
                double divider = helper::var_as_double(args[1]);

                if(divider == 0.0)
                    throw invalid_operation_error("Division by zero error in fmod!");

                return make_ref<variable>(any(std::fmod(helper::var_as_double(args[0]), divider)), variable_type::vt_float);
            }

            static ref<variable_base> b_sinh(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::sinh(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_cosh(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::cosh(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_tanh(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::tanh(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_asinh(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(std::asinh(helper::var_as_double(args[0]))), variable_type::vt_float);
            }

            static ref<variable_base> b_acosh(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);

                if (val < 1.0)
                    throw builtin_error("ArcCosh input must be greater than or equal to 1");

                return make_ref<variable>(any(std::acosh(val)), variable_type::vt_float);
            }

            static ref<variable_base> b_atanh(const std::vector<ref<variable_base>>& args)
            {
                double val = helper::var_as_double(args[0]);
                if (val <= -1.0 || val >= 1.0)
                    throw builtin_error("ArcTanh input must be between -1 and 1 (exclusive)");

                return make_ref<variable>(any(std::atanh(val)), variable_type::vt_float);
            }

            static ref<variable_base> b_root(const std::vector<ref<variable_base>>& args)
            {
                double n_value = helper::var_as_double(args[1]);

                if (n_value == 0.0)
                    throw builtin_error("Cannot take 0th root.");

                return make_ref<variable>(any(std::pow(helper::var_as_double(args[0]), 1.0 / n_value)), variable_type::vt_float);
            }

            static ref<variable_base> b_log_base(const std::vector<ref<variable_base>>& args)
            {
                double value = helper::var_as_double(args[0]);
                double base_value = helper::var_as_double(args[1]);

                if (base_value <= 0.0 || base_value == 1.0)
                    throw builtin_error("Logarithm base cannot be zero, one, or negative.");
                if (value <= 0.0) 
                    throw builtin_error("Logarithm of zero or negative number is undefined.");

                return make_ref<variable>(any(std::log(value) / std::log(base_value)), variable_type::vt_float);
            }

            static ref<variable_base> b_min(const std::vector<ref<variable_base>>& args)
            {
                return (helper::var_as_double(args[0]) < helper::var_as_double(args[1])) ? args[0] : args[1];
            }

            static ref<variable_base> b_max(const std::vector<ref<variable_base>>& args)
            {
                return (helper::var_as_double(args[0]) > helper::var_as_double(args[1])) ? args[0] : args[1];
            }

            static ref<variable_base> b_random(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(get_rman().random(), variable_type::vt_integer);
            }

            static ref<variable_base> b_frandom(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(get_rman().drandom(), variable_type::vt_float);
            }

            static ref<variable_base> b_random_in_range(const std::vector<ref<variable_base>>& args)
            {
                if(args[0]->get_type() == variable_type::vt_integer || args[1]->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(get_rman().int_range(std::dynamic_pointer_cast<variable>(args[0])->get_data_as<long long>(), std::dynamic_pointer_cast<variable>(args[1])->get_data_as<long long>())), variable_type::vt_integer);
                else if (args[0]->get_type() == variable_type::vt_float || args[0]->get_type() == variable_type::vt_float_ref ||
                    args[1]->get_type() == variable_type::vt_float || args[1]->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(get_rman().double_range(helper::var_as_double(args[0]), helper::var_as_double(args[1]))), variable_type::vt_float);
                return create_null_variable();
            }

            static ref<variable_base> b_vec2(const std::vector<ref<variable_base>>& args)
            {
                return helper::create_vec2(args[0], args[1]);
            }

            static ref<variable_base> b_vec2_2(const std::vector<ref<variable_base>>& args)
            {
                return helper::create_vec2(args[0], args[0]);
            }

            static ref<variable_base> b_vec2_3(const std::vector<ref<variable_base>>& args)
            {
                auto ref_0 = make_ref<variable>(any(0.0), variable_type::vt_float);
                return helper::create_vec2(ref_0, ref_0);
            }

            static ref<variable_base> b_vec3(const std::vector<ref<variable_base>>& args)
            {
                return helper::create_vec3(args[0], args[1], args[2]);
            }

            static ref<variable_base> b_vec3_2(const std::vector<ref<variable_base>>& args)
            {
                return helper::create_vec3(args[0], args[0], args[0]);
            }

            static ref<variable_base> b_vec3_3(const std::vector<ref<variable_base>>& args)
            {
                auto var_vec2 = std::dynamic_pointer_cast<variable>(args[0]);
                vec2 data = var_vec2->get_data_as<vec2>();

                auto ref_x = make_ref<variable>(any(data.x), variable_type::vt_float);
                auto ref_y = make_ref<variable>(any(data.y), variable_type::vt_float);

                return helper::create_vec3(ref_x, ref_y, args[1]);
            }

            static ref<variable_base> b_vec3_4(const std::vector<ref<variable_base>>& args)
            {
                auto ref_0 = make_ref<variable>(any(0.0), variable_type::vt_float);
                return helper::create_vec3(ref_0, ref_0, ref_0);
            }
        } // namespace detail

        static void load_all(symbol_map& table)
        {
            using namespace wio::builtin::detail;

            loader::load_function(table, "Abs", detail::b_abs, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Sin", detail::b_sin, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Cos", detail::b_cos, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Tan", detail::b_tan, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Sqrt", detail::b_sqrt, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Pow", detail::b_pow, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Log", detail::b_log, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Log10", detail::b_log10, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcSin", detail::b_asin, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcCos", detail::b_acos, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcTan", detail::b_atan, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Ceil", detail::b_ceil, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Floor", detail::b_floor, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Round", detail::b_round, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Exp", detail::b_exp, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcTan2", detail::b_atan2, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Fmod", detail::b_fmod, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Sinh", detail::b_sinh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Cosh", detail::b_cosh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Tanh", detail::b_tanh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcSinh", detail::b_asinh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcCosh", detail::b_acosh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "ArcTanh", detail::b_atanh, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "Root", detail::b_root, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "LogBase", detail::b_log_base, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Min", detail::b_min, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Max", detail::b_max, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_function(table, "Random", detail::b_random, pa<0>{ });
            loader::load_function(table, "FRandom", detail::b_frandom, pa<0>{ });
            loader::load_function(table, "RandomInRange", detail::b_random_in_range, pa<2>{ variable_type::vt_any, variable_type::vt_any });

            auto vec2_result = loader::load_function(table, "Vec2", detail::b_vec2, pa<2>{ variable_type::vt_any, variable_type::vt_any });
            loader::load_overload(vec2_result, detail::b_vec2_2, pa<1>{ variable_type::vt_any });
            loader::load_overload(vec2_result, detail::b_vec2_3, pa<0>{});

            auto vec3_result = loader::load_function(table, "Vec3", detail::b_vec3, pa<3>{ variable_type::vt_any, variable_type::vt_any, variable_type::vt_any });
            loader::load_overload(vec3_result, detail::b_vec3_2, pa<1>{ variable_type::vt_any });
            loader::load_overload(vec3_result, detail::b_vec3_3, pa<2>{ variable_type::vt_vec2, variable_type::vt_any });
            loader::load_overload(vec3_result, detail::b_vec3_4, pa<0>{});

            // math
            loader::load_constant(table, "NAN", variable_type::vt_float, std::numeric_limits<double>::quiet_NaN());
            loader::load_constant(table, "INF", variable_type::vt_float, std::numeric_limits<double>::infinity());
            loader::load_constant(table, "PI", variable_type::vt_float, 3.14159265358979323846);
            loader::load_constant(table, "E", variable_type::vt_float, 2.71828182845904523536);
            loader::load_constant(table, "LOG2E", variable_type::vt_float, 1.44269504088896340736);
            loader::load_constant(table, "LOG10E", variable_type::vt_float, 0.434294481903251827651);
            loader::load_constant(table, "LN2", variable_type::vt_float, 0.693147180559945309417);
            loader::load_constant(table, "LN10", variable_type::vt_float, 2.30258509299404568402);
            loader::load_constant(table, "SQRT2", variable_type::vt_float, 1.41421356237309504880);
            loader::load_constant(table, "SQRT3", variable_type::vt_float, 1.73205080756887729352);
        }

        void math::load(ref<scope> target_scope)
        {
            if (!target_scope)
                target_scope = main_table::get().get_builtin_scope();

            load_all(target_scope->get_symbols());
        }

        void math::load_table(ref<symbol_table> target_table)
        {
            if (target_table)
                load_all(target_table->get_symbols());
        }

        void math::load_symbol_map(symbol_map& target_map)
        {
            load_all(target_map);
        }
    } // namespace builtin
} // namespace wio