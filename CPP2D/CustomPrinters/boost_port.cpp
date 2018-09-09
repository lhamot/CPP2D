//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <ciso646>
#include <iostream>

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#pragma warning(pop)

#include "../DPrinter.h"
#include "../MatchContainer.h"
#include "../CustomPrinters.h"
#include "../Options.h"

using namespace clang;
using namespace clang::ast_matchers;

void printFormatFuncOrOpCall(DPrinter& pr, Expr* leftOp)
{
	if(auto* funcCast = dyn_cast<CXXFunctionalCastExpr>(leftOp))
		pr.TraverseStmt(funcCast->getSubExpr());
	else if(auto* leftOpCall = dyn_cast<CXXOperatorCallExpr>(leftOp))
	{
		printFormatFuncOrOpCall(pr, leftOpCall->getArg(0));
		if(leftOpCall->getNumArgs() > 1)
		{
			pr.stream() << ", ";
			pr.TraverseStmt(leftOpCall->getArg(1)); //The right of the left
		}
	}

};


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

	//boost::format
	Options::getInstance().types["class boost::basic_format<"].semantic = TypeOptions::Value;

	// ****************************** boost::format ***********************************************
	mc.operatorCallPrinter(finder, "^::boost::basic_format(<|$)", "%", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			pr.addExternInclude("std.format", "std.format.format");
			pr.stream() << "std.format.format(";
			printFormatFuncOrOpCall(pr, opCall->getArg(0)); //Left operand
			if(opCall->getNumArgs() > 1)
			{
				pr.stream() << ", ";
				pr.TraverseStmt(opCall->getArg(1)); //Right operand
			}
			pr.stream() << ")";
		}
	});

	mc.globalFuncPrinter("^::boost::str(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
			pr.TraverseStmt(memCall->getArg(0));
	});


	// ****************************** boost::range ***********************************************
	// transform
	finder.addMatcher(cxxOperatorCallExpr(
	                    hasArgument(1, hasType(namedDecl(matchesName("transform_holder")))),
	                    hasOverloadedOperatorName("|")
	                  ).bind("boost::transformed"), &mc);
	mc.stmtPrinters.emplace("boost::transformed", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			pr.addExternInclude("std.algorithm", "std.algorithm.map");
			pr.TraverseStmt(opCall->getArg(0));
			pr.stream() << "[].map!(";
			if(auto* a = dyn_cast<MaterializeTemporaryExpr>(opCall->getArg(1)))
			{
				if(auto* b = dyn_cast<ImplicitCastExpr>(a->getTemporary()))
				{
					if(auto* c = dyn_cast<CXXOperatorCallExpr>(b->getSubExpr()))
					{
						Expr* right = c->getArg(1);
						pr.dontTakePtr.insert(right);
						pr.TraverseStmt(right);
						pr.dontTakePtr.erase(right);
					}
				}
			}
			pr.stream() << ")()";
		}
	});
}

REG_CUSTOM_PRINTER(boost_port);
