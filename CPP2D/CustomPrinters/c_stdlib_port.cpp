//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#pragma warning(pop)

#include "../DPrinter.h"
#include "../MatchContainer.h"
#include "../CustomPrinters.h"

using namespace clang;
using namespace clang::ast_matchers;

void c_stdlib_port(MatchContainer& mc, MatchFinder& finder)
{
	//************************** C libraries ******************************************************
	// <stdio>
	finder.addMatcher(
	  declRefExpr(hasDeclaration(functionDecl(hasName("::printf")))).bind("printf"), &mc);
	mc.stmtPrinters.emplace("printf", [](DPrinter & printer, Stmt*)
	{
		printer.addExternInclude("std.stdio", "writef");
		printer.stream() << "writef";
	});

	// <string>
	mc.globalFuncPrinter(finder, "^(::std)?::strlen", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc.string.strlen(";
			pr.TraverseStmt(call->getArg(0));
			pr.stream() << ".ptr)";
			pr.addExternInclude("core.stdc.string", "core.stdc.string.strlen");
		}
	});

	// <stdlib>
	mc.cFuncPrinter(finder, "stdlib", "rand");

	// <cmath>
	char const* mathFuncs[] =
	{
		"cos", "sin", "tan", "acos", "asin", "atan", "atan2",
		"cosh", "sinh", "tanh", "acosh", "asinh", "atanh",
		"exp", "frexp", "ldexp", "log", "log10", "modf", "exp2", "expm1", "ilogb", "log1p", "log2", "logb", "scalbn", "scalbln",
		"pow", "sqrt", "cbrt", "hypot",
		"erf", "erfc", "tgamma", "lgamma",
		"ceil", "floor", "fmod", "trunc", "round", "lround", "llround", "rint", "lrint", "llrint", "nearbyint", "remainder", "remquo",
		"copysign", "nan", "nextafter", "nexttoward",
		"fdim", "fmax", "fmin",
		"fabs", "abs", "fma",
		"fpclassify", "isfinite", "isinf", "isnan", "isnormal", "signbit",
		"isgreater", "isgreaterequal", "isless", "islessequal", "islessgreater", "isunordered",
	};
	for(char const* func : mathFuncs)
		mc.cFuncPrinter(finder, "math", func);

	// <ctime>
	mc.cFuncPrinter(finder, "time", "time");

	//<assert>
	mc.globalFuncPrinter(finder, "^(::std)?::_wassert", [](DPrinter&, Stmt*) {});
}

REG_CUSTOM_PRINTER(c_stdlib_port);