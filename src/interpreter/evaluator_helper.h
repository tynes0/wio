#pragma once

#include "../variables/variable_base.h"

namespace wio
{
	namespace helper
	{
		ref<variable_base> eval_binary_exp_addition(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // +
		ref<variable_base> eval_binary_exp_subtraction(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // -
		ref<variable_base> eval_binary_exp_multiplication(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // *
		ref<variable_base> eval_binary_exp_division(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // /
		ref<variable_base> eval_binary_exp_modulo(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // %
		ref<variable_base> eval_binary_exp_assignment(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // =
		ref<variable_base> eval_binary_exp_add_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // +=
		ref<variable_base> eval_binary_exp_subtract_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // -=
		ref<variable_base> eval_binary_exp_multiply_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // *=
		ref<variable_base> eval_binary_exp_divide_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // /=
		ref<variable_base> eval_binary_exp_modulo_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // %=
		ref<variable_base> eval_binary_exp_less_than(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // <
		ref<variable_base> eval_binary_exp_greater_than(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // >
		ref<variable_base> eval_binary_exp_less_than_or_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // <=
		ref<variable_base> eval_binary_exp_greater_than_or_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // >=
		ref<variable_base> eval_binary_exp_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // ==
		ref<variable_base> eval_binary_exp_not_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // !=
		ref<variable_base> eval_binary_exp_type_equal(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // =?
		ref<variable_base> eval_binary_exp_logical_and(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // &&
		ref<variable_base> eval_binary_exp_logical_or(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // ||
		ref<variable_base> eval_binary_exp_bitwise_and(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // &
		ref<variable_base> eval_binary_exp_bitwise_or(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // |
		ref<variable_base> eval_binary_exp_left_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // <<
		ref<variable_base> eval_binary_exp_right_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // >>
		ref<variable_base> eval_binary_exp_left_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // <<=
		ref<variable_base> eval_binary_exp_right_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // >>=
		ref<variable_base> eval_binary_exp_to_left(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // <-
		ref<variable_base> eval_binary_exp_to_right(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc); // ->
	}											   
}