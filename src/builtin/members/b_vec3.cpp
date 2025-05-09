#include "b_vec3.h"

#include "../../types/vec3.h"
#include "../../variables/variable.h"

#include "../builtin_base.h"
#include "../helpers.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static vec3& get_vec3(ref<variable_base> base)
            {
                if (base->get_type() == variable_type::vt_vec3)
                    return std::dynamic_pointer_cast<variable>(base)->get_data_as<vec3>();
                throw type_mismatch_error("Type should be vec2!");
            }

            static ref<variable_base> b_vec3_magnitude_squared(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.magnitude_squared()), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_dot(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.dot(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_cross(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.cross(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_distance_squared(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.distance_squared(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_magnitude(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.magnitude()), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_distance(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.distance(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_angle(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.angle(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec3_normalize(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.normalize()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_normalized(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.normalized()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_is_unit(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.is_unit()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_is_unit_2(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                float_t epsilon = helper::var_as_float(args[1]);

                return make_ref<variable>(any(vec_var.is_unit(epsilon)), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_lerp(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_target = get_vec3(args[1]);
                float_t t = helper::var_as_float(args[1]);

                return make_ref<variable>(any(vec_var.lerp(vec_target, t)), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_clamp(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_min = get_vec3(args[1]);
                vec3& vec_max = get_vec3(args[2]);

                return make_ref<variable>(any(vec_var.clamp(vec_min, vec_max)), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_clamp_2(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                float_t min = helper::var_as_float(args[1]);
                float_t max = helper::var_as_float(args[2]);

                return make_ref<variable>(any(vec_var.clamp(min, max)), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_abs(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.abs()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_floor(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.floor()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_ceil(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.ceil()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_round(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.round()), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_project(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.project(vec_param)), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_reflect(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.reflect(vec_param)), variable_type::vt_vec3);
            }

            static ref<variable_base> b_vec3_is_one_of_zero(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.is_one_of_zero()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_is_zero(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.is_zero()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_is_nan(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.is_nan()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_is_inf(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);

                return make_ref<variable>(any(vec_var.is_infinite()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_equals(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);

                return make_ref<variable>(any(vec_var.equals(vec_param)), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec3_equals_2(const std::vector<ref<variable_base>>& args)
            {
                vec3& vec_var = get_vec3(args[0]);
                vec3& vec_param = get_vec3(args[1]);
                float_t d_param = helper::var_as_float(args[2]);

                return make_ref<variable>(any(vec_var.equals(vec_param, d_param)), variable_type::vt_bool);
            }
        }


        ref<symbol_table> b_vec3::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "MagnitudeSquared", detail::b_vec3_magnitude_squared, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Dot", detail::b_vec3_dot, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Cross", detail::b_vec3_cross, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "DistanceSquared", detail::b_vec3_distance_squared, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Magnitude", detail::b_vec3_magnitude, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Distance", detail::b_vec3_distance, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Angle", detail::b_vec3_angle, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Normalize", detail::b_vec3_normalize, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Normalized", detail::b_vec3_normalized, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "IsUnit", detail::b_vec3_is_unit, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Lerp", detail::b_vec3_lerp, pa<3>{ variable_base_type::variable, variable_base_type::variable, variable_base_type::variable });
            auto clamp_func = loader::load_function(table, "Clamp", detail::b_vec3_clamp, pa<3>{ variable_base_type::variable, variable_base_type::variable, variable_base_type::variable });
            loader::load_overload(clamp_func, detail::b_vec3_clamp_2, pa<3>{ variable_base_type::variable, variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Abs", detail::b_vec3_abs, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Floor", detail::b_vec3_floor, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Ceil", detail::b_vec3_ceil, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Round", detail::b_vec3_round, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Project", detail::b_vec3_project, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Reflect", detail::b_vec3_reflect, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Round", detail::b_vec3_round, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "IsOneOfZero", detail::b_vec3_is_one_of_zero, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "IsZero", detail::b_vec3_is_zero, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "IsNan", detail::b_vec3_is_nan, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "IsInfinite", detail::b_vec3_is_inf, pa<1>{ variable_base_type::variable });
            auto equals_func = loader::load_function(table, "Equals", detail::b_vec3_equals, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_overload(equals_func, detail::b_vec3_equals_2, pa<3>{ variable_base_type::variable, variable_base_type::variable, variable_base_type::variable });


            return result;
        }
    }
}
