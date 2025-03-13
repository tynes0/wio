#pragma once

namespace wio
{
	class application
	{
	public:
		application(int argc, const char** argv);
		void run();
	};
}
// TODOS:
// 1- global var decl fix
// 2- binary exp fix
// 3- builtin constants and methods
// 4- app -help
// 5- app -sf -single-file
// 6- func call array element
// 7- nested arrays
// 8- array - dict copy
// 8- empty func decl
// 10- =? op -> type equal
// 11- ? op -> is not null op -> var x = null; if(?x) {##job} -> if x is not null do the job
// 12- remove is_local - is_global from variable_base