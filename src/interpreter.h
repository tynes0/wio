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
		id_t run_file(const char* fp, packed_bool flags, symbol_map* target_map = nullptr);
		raw_buffer run_file_part_1(const char* fp, packed_bool flags, symbol_map* target_map = nullptr);
		void run_file_part_2(raw_buffer content, packed_bool flags, symbol_map* target_map = nullptr);
		void run_file_part_3();

		friend class evaluator;
	};
}