#pragma once

#include "utils/buffer.h"
#include "base/base.h"
#include "interpreter/scope.h"

namespace wio
{
	class interpreter
	{
	public:
		interpreter();
		void load_args(int argc, char* argv[]);
		void run();

		static interpreter& get();
	private:
		id_t run_f(const char* fp, packed_bool flags, symbol_map* target_map = nullptr);
		raw_buffer run_f_p1(const char* fp, packed_bool flags, symbol_map* target_map = nullptr);
		void run_f_p2(raw_buffer content, packed_bool flags, symbol_map* target_map = nullptr);
		void run_f_p3();

		friend class evaluator;
	};
}