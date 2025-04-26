#include "b_vec2.h"

#include "../../types/vec2.h"
#include "../../variables/variable.h"

#include "../builtin_base.h"
#include "../helpers.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<variable> get_vec2_var(ref<variable_base> base)
            {
                if (!base)
                    throw exception("Variable is null!");

                ref<variable> var = std::dynamic_pointer_cast<variable>(base);

                if (!var)
                    throw type_mismatch_error("Mismatch parameter type!");
                return var;
            }

            static vec2& get_vec2(ref<variable_base> base)
            {
                if (base->get_type() == variable_type::vt_vec2)
                    return std::dynamic_pointer_cast<variable>(base)->get_data_as<vec2>();
                throw type_mismatch_error("Type should be vec2!");
            }

            static ref<variable_base> b_vec2_magnitude_squared(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.magnitude_squared()), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_dot(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.dot(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_cross(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.cross(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_distance_squared(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.distance_squared(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_magnitude(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.magnitude()), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_distance(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.distance(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_angle(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.angle(vec_param)), variable_type::vt_float);
            }

            static ref<variable_base> b_vec2_normalize(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.normalize()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_normalized(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.normalized()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_rotate(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                double rad = helper::var_as_double(args[1]);

                return make_ref<variable>(any(vec_var.rotate(rad)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_rotated(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                double rad = helper::var_as_double(args[1]);

                return make_ref<variable>(any(vec_var.rotated(rad)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_lerp(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_target = get_vec2(args[1]);
                double t = helper::var_as_double(args[1]);

                return make_ref<variable>(any(vec_var.lerp(vec_target, t)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_clamp(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_min = get_vec2(args[1]);
                vec2& vec_max = get_vec2(args[2]);

                return make_ref<variable>(any(vec_var.clamp(vec_min, vec_max)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_abs(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.abs()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_floor(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.floor()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_ceil(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.ceil()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_round(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.round()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_project(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.project(vec_param)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_reflect(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.reflect(vec_param)), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_perpendicular(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.perpendicular()), variable_type::vt_vec2);
            }

            static ref<variable_base> b_vec2_is_one_of_zero(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.is_one_of_zero()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec2_is_zero(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.is_zero()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec2_is_nan(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.is_nan()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec2_is_inf(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);

                return make_ref<variable>(any(vec_var.is_infinite()), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec2_equals(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);

                return make_ref<variable>(any(vec_var.equals(vec_param)), variable_type::vt_bool);
            }

            static ref<variable_base> b_vec2_equals_2(const std::vector<ref<variable_base>>& args)
            {
                vec2& vec_var = get_vec2(args[0]);
                vec2& vec_param = get_vec2(args[1]);
                double d_param = helper::var_as_double(args[2]);

                return make_ref<variable>(any(vec_var.equals(vec_param, d_param)), variable_type::vt_bool);
            }
        }


        ref<symbol_table> b_vec2::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "MagnitudeSquared", detail::b_vec2_magnitude_squared, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Dot", detail::b_vec2_dot, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Cross", detail::b_vec2_cross, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "DistanceSquared", detail::b_vec2_distance_squared, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Magnitude", detail::b_vec2_magnitude, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Distance", detail::b_vec2_distance, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Angle", detail::b_vec2_angle, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Normalize", detail::b_vec2_normalize, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Normalized", detail::b_vec2_normalized, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Rotate", detail::b_vec2_rotate, pa<2>{ variable_type::vt_vec2, variable_type::vt_any });
            loader::load_function(table, "Rotated", detail::b_vec2_rotated, pa<2>{ variable_type::vt_vec2, variable_type::vt_any });
            loader::load_function(table, "Lerp", detail::b_vec2_lerp, pa<3>{ variable_type::vt_vec2, variable_type::vt_vec2, variable_type::vt_any });
            loader::load_function(table, "Clamp", detail::b_vec2_clamp, pa<3>{ variable_type::vt_vec2, variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Abs", detail::b_vec2_abs, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Floor", detail::b_vec2_floor, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Ceil", detail::b_vec2_ceil, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Round", detail::b_vec2_round, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "Project", detail::b_vec2_project, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Reflect", detail::b_vec2_reflect, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_function(table, "Round", detail::b_vec2_round, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "IsOneOfZero", detail::b_vec2_is_one_of_zero, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "IsZero", detail::b_vec2_is_zero, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "IsNan", detail::b_vec2_is_nan, pa<1>{ variable_type::vt_vec2 });
            loader::load_function(table, "IsInfinite", detail::b_vec2_is_inf, pa<1>{ variable_type::vt_vec2 });

            auto equals_func = loader::load_function(table, "Equals", detail::b_vec2_equals, pa<2>{ variable_type::vt_vec2, variable_type::vt_vec2 });
            loader::load_overload(equals_func, detail::b_vec2_equals_2, pa<3>{ variable_type::vt_vec2, variable_type::vt_vec2, variable_type::vt_any });


            return result;
        }
    }
}
