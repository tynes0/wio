#pragma once

#include "utils/buffer.h"

namespace wio
{
	class interpreter
	{
	public:
		interpreter();
		void load_args(int argc, const char** argv);
		void run();

		static interpreter& get();
	private:
		raw_buffer run_f(const char* fp, bool sf = false);
		raw_buffer run_f_p1(const char* fp);
		raw_buffer run_f_p2(raw_buffer content, bool sf);
		void run_f_p3();

		friend class evaluator;
	};
}