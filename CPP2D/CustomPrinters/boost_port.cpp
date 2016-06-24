//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <ciso646>

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#pragma warning(pop)

#include "../DPrinter.h"
#include "../MatchContainer.h"
#include "../CustomPrinters.h"

using namespace clang;
using namespace clang::ast_matchers;

//! @todo This function has to be split across all includes
void boost_port(MatchContainer& mc, MatchFinder& finder)
{
	//**************************** boost **********************************************************

	//BOOST_THROW_EXCEPTION
	mc.globalFuncPrinter("throw_exception_(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "throw ";
			pr.TraverseStmt(*memCall->arg_begin());
		}
	});

	//boost::serialisation
	mc.rewriteType(finder, "boost::serialization::access", "int", "");

	finder.addMatcher(
	  forStmt(
	    hasLoopInit(declStmt(hasSingleDecl(varDecl(hasType(namedDecl(matchesName("record"))))))))
	  .bind("boost::log"),
	  &mc);
	mc.stmtPrinters.emplace("boost::log", [](DPrinter & pr, Stmt * s)
	{
		if(auto* forSt = dyn_cast<ForStmt>(s))
		{
			pr.TraverseStmt(forSt->getBody());
			pr.stream() << ';';
		}
	});
}

REG_CUSTOM_PRINTER(boost_port);