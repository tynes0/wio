#include "math.h"
#include <cmath>
#include <stdexcept>
#include <sstream>

#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../variables/function.h"
#include "../base/exception.h"
#include "../utils/util.h"

#include "builtin_base.h"

namespace wio 
{
    namespace builtin 
    {
        namespace detail 
        {

            static std::string make_type_error_message(const std::string& func_name, const std::string& expected_types, const variable_type& actual_type)
            {
                std::stringstream ss;
                std::string s = util::type_to_string(actual_type);
                ss << "Invalid parameter type for '" << func_name << "'. Expected: " << expected_types << ", but got: " << s;
                return ss.str();
            }

            static ref<variable> b_abs(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_integer) 
                    return make_ref<variable>(any(std::abs(value->get_data_as<long long>())), variable_type::vt_integer);
                else if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::abs(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::abs(*value->get_data_as<double*>())), variable_type::vt_float);
                throw builtin_error(make_type_error_message("Abs", "integer or float", type));
            }

            static ref<variable> b_sin(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::sin(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref) 
                    return make_ref<variable>(any(std::sin(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer) 
                    return make_ref<variable>(any(std::sin(value->get_data_as<long long>())), variable_type::vt_float);
                throw builtin_error(make_type_error_message("Sin", "float", type));
            }

            static ref<variable> b_cos(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::cos(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::cos(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::cos(value->get_data_as<long long>())), variable_type::vt_float);
                throw builtin_error(make_type_error_message("Cos", "float", type));
            }

            static ref<variable> b_tan(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::tan(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::tan(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::tan(value->get_data_as<long long>())), variable_type::vt_float);
                throw builtin_error(make_type_error_message("Tan", "float", type));
            }

            static ref<variable> b_sqrt(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float)
                {
                    double val = value->get_data_as<double>();
                    if (val < 0.0)
                        throw builtin_error("Cannot take square root of negative number");
                    return make_ref<variable>(any(std::sqrt(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();
                    if (val < 0)
                        throw builtin_error("Cannot take square root of negative number");
                    return make_ref<variable>(any(std::sqrt(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer)
                {
                    long long val = value->get_data_as<long long>();
                    if (val < 0)
                        throw builtin_error("Cannot take square root of negative number");
                    return make_ref<variable>(any(std::sqrt(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("Sqrt", "float or integer", type));
            }

            static ref<variable> b_pow(const ref<variable_base>& value_base, const ref<variable_base>& exponent_base)
            {
                ref<variable> base = std::dynamic_pointer_cast<variable>(value_base);
                ref<variable> exponent = std::dynamic_pointer_cast<variable>(exponent_base);
                auto base_type = base->get_type();
                auto exp_type = exponent->get_type();

                if (base_type == variable_type::vt_float)
                {
                    if (exp_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(base->get_data_as<double>(), exponent->get_data_as<double>())), variable_type::vt_float);
                    else if(exp_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(base->get_data_as<double>(), *exponent->get_data_as<double*>())), variable_type::vt_float);
                    else if(exp_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(base->get_data_as<double>(), exponent->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (base_type == variable_type::vt_float_ref)
                {
                    if (exp_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(*base->get_data_as<double*>(), exponent->get_data_as<double>())), variable_type::vt_float);
                    else if (exp_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(*base->get_data_as<double*>(), *exponent->get_data_as<double*>())), variable_type::vt_float);
                    else if (exp_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(*base->get_data_as<double*>(), exponent->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (base_type == variable_type::vt_integer)
                {
                    if (exp_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(base->get_data_as<long long>(), exponent->get_data_as<long long>())), variable_type::vt_float);
                    else if (exp_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(base->get_data_as<long long>(), exponent->get_data_as<double>())), variable_type::vt_float);
                    else if (exp_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(base->get_data_as<long long>(), *exponent->get_data_as<double*>())), variable_type::vt_float);
                }

                throw builtin_error("Invalid types for Pow.  Expected (float, float), (int, int), (float, int) or (int, float)");
            }

            static ref<variable> b_log(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                {
                    double val = value->get_data_as<double>();

                    if (val <= 0.0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val <= 0.0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer) 
                {
                    long long val = value->get_data_as<long long>();

                    if (val <= 0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("Log", "float or integer", type));
            }

            static ref<variable> b_log10(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                {
                    double val = value->get_data_as<double>();

                    if (val <= 0.0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log10(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val <= 0.0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log10(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer)
                {
                    long long val = value->get_data_as<long long>();

                    if (val <= 0)
                        throw builtin_error("Logarithm of zero or negative number is undefined.");

                    return make_ref<variable>(any(std::log10(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("Log10", "float or integer", type));
            }

            static ref<variable> b_asin(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();

                if (type == variable_type::vt_float)
                {
                    double val = value->get_data_as<double>();

                    if (val < -1.0 || val > 1.0) 
                        throw builtin_error("ArcSin input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::asin(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val < -1.0 || val > 1.0)
                        throw builtin_error("ArcSin input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::asin(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer)
                {
                    long long val = value->get_data_as<long long>();

                    if (val < -1 || val > 1)
                        throw builtin_error("ArcSin input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::asin(val)), variable_type::vt_float);
                }

                throw builtin_error(make_type_error_message("ArcSin", "float or integer", type));
            }

            static ref<variable> b_acos(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float)
                {
                    double val = value->get_data_as<double>();

                    if (val < -1.0 || val > 1.0)
                        throw builtin_error("ArcCos input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::acos(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val < -1.0 || val > 1.0)
                        throw builtin_error("ArcCos input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::acos(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer)
                {
                    long long val = value->get_data_as<long long>();

                    if (val < -1 || val > 1)
                        throw builtin_error("ArcCos input must be in the range [-1, 1]");

                    return make_ref<variable>(any(std::acos(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("ArcCos", "float or integer", type));
            }

            static ref<variable> b_atan(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();

                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::atan(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::atan(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::atan(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("ArcTan", "float or integer", type));
            }

            static ref<variable> b_ceil(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();

                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::ceil(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::ceil(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(value->get_data_as<long long>()), variable_type::vt_integer);

                throw builtin_error(make_type_error_message("Ceil", "float or integer", type));
            }

            static ref<variable> b_floor(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();

                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::floor(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::floor(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(value->get_data_as<long long>()), variable_type::vt_integer);

                throw builtin_error(make_type_error_message("Floor", "float or integer", type));
            }

            static ref<variable> b_round(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();

                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::round(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::round(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(value->get_data_as<long long>()), variable_type::vt_integer);

                throw builtin_error(make_type_error_message("Round", "float or integer", type));
            }


            static ref<variable> b_exp(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::exp(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::exp(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::exp(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("Exp", "float or integer", type));
            }

            static ref<variable> b_atan2(const ref<variable_base>& y_base, const ref<variable_base>& x_base)
            {
                ref<variable> y = std::dynamic_pointer_cast<variable>(y_base);
                ref<variable> x = std::dynamic_pointer_cast<variable>(x_base);
                auto y_type = y->get_type();
                auto x_type = x->get_type();

                if (y_type == variable_type::vt_float)
                {
                    if(x_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<double>(), x->get_data_as<double>())), variable_type::vt_float);
                    else if(x_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<double>(), *x->get_data_as<double*>())), variable_type::vt_float);
                    else if(x_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<double>(), x->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (y_type == variable_type::vt_float_ref)
                {
                    if (x_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::atan2(*y->get_data_as<double*>(), x->get_data_as<double>())), variable_type::vt_float);
                    else if (x_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::atan2(*y->get_data_as<double*>(), *x->get_data_as<double*>())), variable_type::vt_float);
                    else if (x_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::atan2(*y->get_data_as<double*>(), x->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (y_type == variable_type::vt_integer)
                {
                    if (x_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<long long>(), x->get_data_as<double>())), variable_type::vt_float);
                    else if (x_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<long long>(), *x->get_data_as<double*>())), variable_type::vt_float);
                    else if (x_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::atan2(y->get_data_as<long long>(), x->get_data_as<long long>())), variable_type::vt_float);
                }

                throw builtin_error("Invalid types for ArcTan2. Expected (float, float), (int, int), (float, int) or (int, float)");
            }

            static ref<variable> b_fmod(const ref<variable_base>& value_base, const ref<variable_base>& divisor_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                ref<variable> divisor = std::dynamic_pointer_cast<variable>(divisor_base);

                auto value_type = value->get_type();
                auto divisor_type = divisor->get_type();

                if (value_type == variable_type::vt_float)
                {
                    if (divisor_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<double>(), divisor->get_data_as<double>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<double>(), *divisor->get_data_as<double*>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<double>(), divisor->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_float_ref)
                {
                    if (divisor_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::fmod(*value->get_data_as<double*>(), divisor->get_data_as<double>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::fmod(*value->get_data_as<double*>(), *divisor->get_data_as<double*>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::fmod(*value->get_data_as<double*>(), divisor->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_integer)
                {
                    if (divisor_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<long long>(), divisor->get_data_as<double>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<long long>(), *divisor->get_data_as<double*>())), variable_type::vt_float);
                    else if (divisor_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::fmod(value->get_data_as<long long>(), divisor->get_data_as<long long>())), variable_type::vt_float);
                }

                throw builtin_error("Invalid types for Fmod. Expected (float, float), (int, int), (float, int), or (int, float)");
            }

            static ref<variable> b_sinh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::sinh(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::sinh(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer) 
                    return make_ref<variable>(any(std::sinh(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("Sinh", "float or integer", type));
            }

            static ref<variable> b_cosh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::cosh(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::cosh(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::cosh(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("Cosh", "float or integer", type));
            }

            static ref<variable> b_tanh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float)
                    return make_ref<variable>(any(std::tanh(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::tanh(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::tanh(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("Tanh", "float or integer", type));
            }

            static ref<variable> b_asinh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                    return make_ref<variable>(any(std::asinh(value->get_data_as<double>())), variable_type::vt_float);
                else if (type == variable_type::vt_float_ref)
                    return make_ref<variable>(any(std::asinh(*value->get_data_as<double*>())), variable_type::vt_float);
                else if (type == variable_type::vt_integer)
                    return make_ref<variable>(any(std::asinh(value->get_data_as<long long>())), variable_type::vt_float);

                throw builtin_error(make_type_error_message("ArcSinh", "float or integer", type));
            }

            static ref<variable> b_acosh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                {
                    double val = value->get_data_as<double>();

                    if (val < 1.0)
                        throw builtin_error("ArcCosh input must be greater than or equal to 1");

                    return make_ref<variable>(any(std::acosh(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val < 1.0)
                        throw builtin_error("ArcCosh input must be greater than or equal to 1");

                    return make_ref<variable>(any(std::acosh(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer) 
                {
                    long long val = value->get_data_as<long long>();

                    if (val < 1) 
                        throw builtin_error("ArcCosh input must be greater than or equal to 1");

                    return make_ref<variable>(any(std::acosh(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("ArcCosh", "float or integer", type));
            }

            static ref<variable> b_atanh(const ref<variable_base>& value_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                auto type = value->get_type();
                if (type == variable_type::vt_float) 
                {
                    double val = value->get_data_as<double>();
                    if (val <= -1.0 || val >= 1.0)
                        throw builtin_error("ArcTanh input must be between -1 and 1 (exclusive)");

                    return make_ref<variable>(any(std::atanh(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_float_ref)
                {
                    double val = *value->get_data_as<double*>();

                    if (val <= -1.0 || val >= 1.0)
                        throw builtin_error("ArcTanh input must be between -1 and 1 (exclusive)");

                    return make_ref<variable>(any(std::atanh(val)), variable_type::vt_float);
                }
                else if (type == variable_type::vt_integer)
                { 
                    long long val = value->get_data_as<long long>();

                    if (val <= -1 || val >= 1) 
                        throw builtin_error("ArcTanh input must be between -1 and 1 (exclusive)");

                    return make_ref<variable>(any(std::atanh(val)), variable_type::vt_float);
                }
                throw builtin_error(make_type_error_message("ArcTanh", "float or integer", type));
            }

            static ref<variable> b_root(const ref<variable_base>& value_base, const ref<variable_base>& n_base) 
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                ref<variable> n = std::dynamic_pointer_cast<variable>(n_base);

                auto value_type = value->get_type();
                auto n_type = n->get_type();

                if ((n_type == variable_type::vt_integer && n->get_data_as<long long>() == 0) || 
                    (n_type == variable_type::vt_float && n->get_data_as<double>() == 0.0) || 
                    (n_type == variable_type::vt_float_ref && *n->get_data_as<double*>() == 0.0))
                    throw builtin_error("Cannot take 0th root.");

                if (value_type == variable_type::vt_float)
                {
                    if(n_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(value->get_data_as<double>(), 1.0 / n->get_data_as<double>())), variable_type::vt_float);
                    else if(n_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(value->get_data_as<double>(), 1.0 / *n->get_data_as<double*>())), variable_type::vt_float);
                    else if(n_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(value->get_data_as<double>(), 1.0 / static_cast<double>(n->get_data_as<long long>()))), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_float_ref)
                {
                    if (n_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(*value->get_data_as<double*>(), 1.0 / n->get_data_as<double>())), variable_type::vt_float);
                    else if (n_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(*value->get_data_as<double*>(), 1.0 / *n->get_data_as<double*>())), variable_type::vt_float);
                    else if (n_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(*value->get_data_as<double*>(), 1.0 / static_cast<double>(n->get_data_as<long long>()))), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_integer)
                {
                    if(n_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::pow(value->get_data_as<long long>(), 1.0 / n->get_data_as<double>())), variable_type::vt_float);
                    else if(n_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::pow(value->get_data_as<long long>(), 1.0 / *n->get_data_as<double*>())), variable_type::vt_float);
                    else if(n_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::pow(value->get_data_as<long long>(), 1.0 / static_cast<double>(n->get_data_as<long long>()))), variable_type::vt_float);
                }

                throw builtin_error("Invalid types for Root. Expected (float, float), (int, int), (float, int) or (int, float)");
            }

            static ref<variable> b_log_base(const ref<variable_base>& value_base, const ref<variable_base>& base_base)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(value_base);
                ref<variable> base = std::dynamic_pointer_cast<variable>(base_base);

                auto value_type = value->get_type();
                auto base_type = base->get_type();

                if ((base_type == variable_type::vt_integer && (base->get_data_as<long long>() <= 0 || base->get_data_as<long long>() == 1)) ||
                    (base_type == variable_type::vt_float && (base->get_data_as<double>() <= 0.0 || base->get_data_as<double>() == 1.0)) || 
                    (base_type == variable_type::vt_float_ref && (*base->get_data_as<double*>() <= 0.0 || *base->get_data_as<double*>() == 1.0)))
                    throw builtin_error("Logarithm base cannot be zero, one, or negative.");
                if ((value_type == variable_type::vt_integer && value->get_data_as<long long>() <= 0) ||
                    (value_type == variable_type::vt_float && value->get_data_as<double>() <= 0.0) ||  
                    (value_type == variable_type::vt_float_ref && *value->get_data_as<double*>() <= 0.0)) 
                    throw builtin_error("Logarithm of zero or negative number is undefined.");

                if (value_type == variable_type::vt_float)
                {
                    if (base_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::log(value->get_data_as<double>()) / std::log(base->get_data_as<double>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::log(value->get_data_as<double>()) / std::log(*base->get_data_as<double*>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::log(value->get_data_as<double>()) / std::log(base->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_float_ref)
                {
                    if (base_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::log(*value->get_data_as<double*>()) / std::log(base->get_data_as<double>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::log(*value->get_data_as<double*>()) / std::log(*base->get_data_as<double*>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::log(*value->get_data_as<double*>()) / std::log(base->get_data_as<long long>())), variable_type::vt_float);
                }
                else if (value_type == variable_type::vt_integer)
                {
                    if (base_type == variable_type::vt_float)
                        return make_ref<variable>(any(std::log(value->get_data_as<long long>()) / std::log(base->get_data_as<double>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_float_ref)
                        return make_ref<variable>(any(std::log(value->get_data_as<long long>()) / std::log(*base->get_data_as<double*>())), variable_type::vt_float);
                    else if (base_type == variable_type::vt_integer)
                        return make_ref<variable>(any(std::log(value->get_data_as<long long>()) / std::log(base->get_data_as<long long>())), variable_type::vt_float);
                }

                throw builtin_error("Invalid types for LogBase. Expected (float, float), (int, int), (float, int) or (int, float)");
            }

            static ref<variable> b_min(const ref<variable_base>& a_base, const ref<variable_base>& b_base)
            {
                ref<variable> a = std::dynamic_pointer_cast<variable>(a_base);
                ref<variable> b = std::dynamic_pointer_cast<variable>(b_base);
                auto a_type = a->get_type();
                auto b_type = b->get_type();

                if (a_type == variable_type::vt_float)
                {
                    double d_a = a->get_data_as<double>();
                    if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();
                        return make_ref<variable>(any(d_a < d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();
                        return make_ref<variable>(any(d_a < d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        any result;
                        variable_type type;
                        if (d_a < double(ll_b))
                        {
                            result = d_a;
                            type = variable_type::vt_float;
                        }
                        else
                        {
                            result = ll_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                }
                else if (a_type == variable_type::vt_float_ref)
                {
                    double d_a = *a->get_data_as<double*>();

                    if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();
                        return make_ref<variable>(any(d_a < d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();
                        return make_ref<variable>(any(d_a < d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        any result;
                        variable_type type;
                        if (d_a < double(ll_b))
                        {
                            result = d_a;
                            type = variable_type::vt_float;
                        }
                        else
                        {
                            result = ll_b;
                            type = variable_type::vt_integer;
                        }
                        return make_ref<variable>(result, type);
                    }
                }
                else if (a_type == variable_type::vt_integer)
                {
                    long long ll_a = a->get_data_as<long long>();

                    if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        return make_ref<variable>(any(ll_a < ll_b ? ll_a : ll_b), variable_type::vt_integer);
                    }
                    else if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();

                        any result;
                        variable_type type;

                        if (ll_a < long long(d_b))
                        {
                            result = ll_a;
                            type = variable_type::vt_integer;
                        }
                        else
                        {
                            result = d_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();

                        any result;
                        variable_type type;

                        if (ll_a < long long(d_b))
                        {
                            result = ll_a;
                            type = variable_type::vt_integer;
                        }
                        else
                        {
                            result = d_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                }

                throw builtin_error("Invalid types for Min. Expected (float, float), (int, int), (float, int) or (int, float)");
            }

            static ref<variable> b_max(const ref<variable_base>& a_base, const ref<variable_base>& b_base)
            {
                ref<variable> a = std::dynamic_pointer_cast<variable>(a_base);
                ref<variable> b = std::dynamic_pointer_cast<variable>(b_base);
                auto a_type = a->get_type();
                auto b_type = b->get_type();

                if (a_type == variable_type::vt_float)
                {
                    double d_a = a->get_data_as<double>();
                    if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();
                        return make_ref<variable>(any(d_a > d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();
                        return make_ref<variable>(any(d_a > d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        any result;
                        variable_type type;
                        if (d_a > double(ll_b))
                        {
                            result = d_a;
                            type = variable_type::vt_float;
                        }
                        else
                        {
                            result = ll_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                }
                else if (a_type == variable_type::vt_float_ref)
                {
                    double d_a = *a->get_data_as<double*>();

                    if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();
                        return make_ref<variable>(any(d_a > d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();
                        return make_ref<variable>(any(d_a > d_b ? d_a : d_b), variable_type::vt_float);
                    }
                    else if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        any result;
                        variable_type type;
                        if (d_a > double(ll_b))
                        {
                            result = d_a;
                            type = variable_type::vt_float;
                        }
                        else
                        {
                            result = ll_b;
                            type = variable_type::vt_integer;
                        }
                        return make_ref<variable>(result, type);
                    }
                }
                else if (a_type == variable_type::vt_integer)
                {
                    long long ll_a = a->get_data_as<long long>();

                    if (b_type == variable_type::vt_integer)
                    {
                        long long ll_b = b->get_data_as<long long>();
                        return make_ref<variable>(any(ll_a > ll_b ? ll_a : ll_b), variable_type::vt_integer);
                    }
                    else if (b_type == variable_type::vt_float)
                    {
                        double d_b = b->get_data_as<double>();

                        any result;
                        variable_type type;

                        if (ll_a > long long(d_b))
                        {
                            result = ll_a;
                            type = variable_type::vt_integer;
                        }
                        else
                        {
                            result = d_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                    else if (b_type == variable_type::vt_float_ref)
                    {
                        double d_b = *b->get_data_as<double*>();

                        any result;
                        variable_type type;

                        if (ll_a > long long(d_b))
                        {
                            result = ll_a;
                            type = variable_type::vt_integer;
                        }
                        else
                        {
                            result = d_b;
                            type = variable_type::vt_float;
                        }
                        return make_ref<variable>(result, type);
                    }
                }
                throw builtin_error("Invalid types for Max. Expected (float, float), (int, int), (float, int) or (int, float)");
            }
        } // namespace detail

        void math::load(ref<scope> target_scope)
        {
            if (!target_scope)
                target_scope = builtin_scope;

            loader::load_function<1>(target_scope, "Abs",     detail::b_abs,      { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Sin",     detail::b_sin,      { variable_type::vt_float });
            loader::load_function<1>(target_scope, "Cos",     detail::b_cos,      { variable_type::vt_float });
            loader::load_function<1>(target_scope, "Tan",     detail::b_tan,      { variable_type::vt_float });
            loader::load_function<1>(target_scope, "Sqrt",    detail::b_sqrt,     { variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "Pow",     detail::b_pow,      { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Log",     detail::b_log,      { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Log10",   detail::b_log10,    { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "ArcSin",  detail::b_asin,     { variable_type::vt_float });
            loader::load_function<1>(target_scope, "ArcCos",  detail::b_acos,     { variable_type::vt_float });
            loader::load_function<1>(target_scope, "ArcTan",  detail::b_atan,     { variable_type::vt_float });
            loader::load_function<1>(target_scope, "Ceil",    detail::b_ceil,     { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Floor",   detail::b_floor,    { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Round",   detail::b_round,    { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Exp",     detail::b_exp,      { variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "ArcTan2", detail::b_atan2,    { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "Fmod",    detail::b_fmod,     { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Sinh",    detail::b_sinh,     { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Cosh",    detail::b_cosh,     { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "Tanh",    detail::b_tanh,     { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "ArcSinh", detail::b_asinh,    { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "ArcCosh", detail::b_acosh,    { variable_type::vt_var_param });
            loader::load_function<1>(target_scope, "ArcTanh", detail::b_atanh,    { variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "Root",    detail::b_root,     { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "LogBase", detail::b_log_base, { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "Min",     detail::b_min,      { variable_type::vt_var_param, variable_type::vt_var_param });
            loader::load_function<2>(target_scope, "Max",     detail::b_max,      { variable_type::vt_var_param, variable_type::vt_var_param });

            // math
            loader::load_constant(target_scope, "NAN",    variable_type::vt_float, std::numeric_limits<double>::quiet_NaN());
            loader::load_constant(target_scope, "INF",    variable_type::vt_float, std::numeric_limits<double>::infinity());
            loader::load_constant(target_scope, "PI",     variable_type::vt_float, 3.14159265358979323846);
            loader::load_constant(target_scope, "E",      variable_type::vt_float, 2.71828182845904523536);
            loader::load_constant(target_scope, "LOG2E",  variable_type::vt_float, 1.44269504088896340736);
            loader::load_constant(target_scope, "LOG10E", variable_type::vt_float, 0.434294481903251827651);
            loader::load_constant(target_scope, "LN2",    variable_type::vt_float, 0.693147180559945309417);
            loader::load_constant(target_scope, "LN10",   variable_type::vt_float, 2.30258509299404568402);
            loader::load_constant(target_scope, "SQRT2",  variable_type::vt_float, 1.41421356237309504880);
            loader::load_constant(target_scope, "SQRT3",  variable_type::vt_float, 1.73205080756887729352);

            // science
            loader::load_constant(target_scope, "GRAVITY",            variable_type::vt_float, 6.67430e-11);
            loader::load_constant(target_scope, "STANDARD_GRAVITY",   variable_type::vt_float, 9.80665);
            loader::load_constant(target_scope, "SPEED_OF_SOUND",     variable_type::vt_float, 340.29);
            loader::load_constant(target_scope, "LIGHT_SPEED",        variable_type::vt_float, 299792458.0);
            loader::load_constant(target_scope, "PHI",                variable_type::vt_float, 1.61803398874989484820);
            loader::load_constant(target_scope, "GOLDEN_RATIO",       variable_type::vt_float, 1.61803398874989484820);
            loader::load_constant(target_scope, "GAMMA",              variable_type::vt_float, 0.57721566490153286060);
            loader::load_constant(target_scope, "FEIGENBAUM_DELTA",   variable_type::vt_float, 4.66920160910299067185);
            loader::load_constant(target_scope, "FEIGENBAUM_ALPHA",   variable_type::vt_float, 2.50290787509589282228);
            loader::load_constant(target_scope, "CATALAN",            variable_type::vt_float, 0.91596559417721901505);
            loader::load_constant(target_scope, "APERYS_CONSTANT",    variable_type::vt_float, 1.20205690315959428540);
            loader::load_constant(target_scope, "PLANCK",             variable_type::vt_float, 6.62607015e-34);
            loader::load_constant(target_scope, "BOLTZMANN",          variable_type::vt_float, 1.380649e-23);
            loader::load_constant(target_scope, "AVOGADRO",           variable_type::vt_float, 6.02214076e23);
            loader::load_constant(target_scope, "MU0",                variable_type::vt_float, 1.25663706212e-6);
            loader::load_constant(target_scope, "EPSILON0",           variable_type::vt_float, 8.8541878128e-12);
            loader::load_constant(target_scope, "RYDBERG",            variable_type::vt_float, 10973731.568160);
            loader::load_constant(target_scope, "BOHR_RADIUS",        variable_type::vt_float, 5.29177210903e-11);
            loader::load_constant(target_scope, "ELECTRON_MASS",      variable_type::vt_float, 9.1093837015e-31);
            loader::load_constant(target_scope, "PROTON_MASS",        variable_type::vt_float, 1.67262192369e-27);
            loader::load_constant(target_scope, "NEUTRON_MASS",       variable_type::vt_float, 1.67492749804e-27);
        }
    } // namespace builtin
} // namespace wio