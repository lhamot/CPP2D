//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "MatchContainer.h"
#include "DPrinter.h"
#include <iostream>
#include <ciso646>

namespace clang
{
namespace ast_matchers
{
const internal::VariadicDynCastAllOfMatcher<Stmt, UnresolvedLookupExpr>
unresolvedLookupExpr;

#pragma warning(push)
#pragma warning(disable: 4100)
AST_MATCHER_P(UnresolvedLookupExpr, uleMatchesName, std::string, RegExp)
{
	assert(!RegExp.empty());
	std::string FullNameString = "::" + Node.getName().getAsString();
	llvm::Regex RE(RegExp);
	return RE.match(FullNameString);
}
#pragma warning(pop)
}
}

using namespace clang;
using namespace clang::ast_matchers;

//! Get the Nth template argument of this type
TemplateArgument const* getTypeTemplateArgument(Expr const* e, size_t tmplArgIndex)
{
	if(auto* cast = dyn_cast<ImplicitCastExpr>(e))
	{
		if(auto* ref = dyn_cast<DeclRefExpr>(cast->getSubExpr()))
		{
			TemplateArgumentLoc const* tmpArgs = ref->getTemplateArgs();
			if(tmpArgs)
			{
				assert(ref->getNumTemplateArgs() > tmplArgIndex);
				return &tmpArgs[tmplArgIndex].getArgument();
			}
			else
			{
				if(auto elType = dyn_cast<ElaboratedType>(ref->getType()))
				{
					if(auto* TSType = dyn_cast<TemplateSpecializationType>(elType->getNamedType()))
					{
						assert(TSType->getNumTemplateArgs() > tmplArgIndex);
						return &TSType->getArg((unsigned int)tmplArgIndex);
					}
					else
						return nullptr;
				}
				else
					return nullptr;
			}
		}
		else
			return nullptr;
	}
	else
		return nullptr;
};

void MatchContainer::rewriteType(clang::ast_matchers::MatchFinder& finder,
                                 std::string const& oldName,
                                 std::string const& newName)
{
	using namespace clang::ast_matchers;
	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName(oldName))))
	                  .bind(oldName), this);
	typePrinters.emplace(oldName, [newName](DPrinter & pr, Type*) {pr.stream() << newName; });
};

void MatchContainer::methodPrinter(std::string const& className,
                                   std::string const& methodName,
                                   StmtPrinter const&  printer)
{
	methodPrinters[methodName].emplace(className, printer);
};

void MatchContainer::globalFuncPrinter(
  clang::ast_matchers::MatchFinder& finder,
  std::string const& funcName,
  StmtPrinter const& printer)
{
	std::string const tag = "globalFuncPrinter_" + funcName;
	finder.addMatcher(callExpr(callee(functionDecl(matchesName(funcName)))).bind(tag), this);
	finder.addMatcher(
	  callExpr(callee(unresolvedLookupExpr(uleMatchesName(funcName)))).bind(tag), this);
	stmtPrinters.emplace(tag, printer);
};

void MatchContainer::tmplTypePrinter(clang::ast_matchers::MatchFinder& finder,
                                     std::string const& name,
                                     TypePrinter const& printer)
{
	finder.addMatcher(templateSpecializationType(hasDeclaration(namedDecl(hasName(name)))).bind(name), this);
	typePrinters.emplace(name, printer);
};

void MatchContainer::memberPrinter(clang::ast_matchers::MatchFinder& finder,
                                   std::string const& memberName,
                                   StmtPrinter const& printer)
{
	std::string const tag = "memberPrinter_" + memberName;
	finder.addMatcher(memberExpr(member(matchesName(memberName))).bind(tag), this);
	stmtPrinters.emplace(tag, printer);
};

void MatchContainer::operatorCallPrinter(
  clang::ast_matchers::MatchFinder& finder,
  std::string const& classRegexpr,
  std::string const& op,
  StmtPrinter const& printer)
{
	std::string const tag = "operatorCallPrinter_" + classRegexpr + op;
	finder.addMatcher(cxxOperatorCallExpr(
	                    hasArgument(0, hasType(cxxRecordDecl(isSameOrDerivedFrom(matchesName(classRegexpr))))),
	                    hasOverloadedOperatorName(op)
	                  ).bind(tag), this);
	stmtPrinters.emplace(tag, printer);
};

void MatchContainer::cFuncPrinter(
  clang::ast_matchers::MatchFinder& finder,
  std::string const& lib,
  std::string const& func)
{
	globalFuncPrinter(finder, "^(::std)?::" + func, [lib, func](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc." << lib << "." << func;
			pr.printCallExprArgument(call);
			pr.addExternInclude("core.stdc." + lib, "core.stdc." + lib + "." + func);
		}
	});
};


clang::ast_matchers::MatchFinder MatchContainer::getMatcher()
{
	MatchFinder finder;

	// Some debug bind slot
	onStmtMatch.emplace("dump", [](Stmt const * d) {d->dump(); });
	onTypeMatch.emplace("dump", [](Type const * d) {d->dump(); });
	onDeclMatch.emplace("dump", [](Decl const * d) {d->dump(); });
	onDeclMatch.emplace("print_name", [](Decl const * d)
	{
		if(auto* nd = dyn_cast<NamedDecl>(d))
			llvm::errs() << nd->getNameAsString() << "\n";
	});

	// std::hash
	DeclarationMatcher hash_trait =
	  namespaceDecl(
	    allOf(
	      hasName("std"),
	      hasDescendant(
	        classTemplateSpecializationDecl(
	          allOf(
	            templateArgumentCountIs(1),
	            hasName("hash"),
	            hasMethod(cxxMethodDecl(hasName("operator()")).bind("hash_method"))
	          )
	        ).bind("dont_print_this_decl")
	      )
	    )
	  );
	finder.addMatcher(hash_trait, this);
	declPrinters.emplace("dont_print_this_decl", [](DPrinter&, Decl*) {});
	onDeclMatch.emplace("hash_method", [this](Decl const * d)
	{
		if(auto* methDecl = dyn_cast<CXXMethodDecl>(d))
		{
			auto* tmplClass = dyn_cast<ClassTemplateSpecializationDecl>(methDecl->getParent());
			TemplateArgumentList const& tmpArgs = tmplClass->getTemplateArgs();
			if(tmpArgs.size() == 1)
			{
				auto const type_name = tmpArgs[0].getAsType().getCanonicalType().getAsString();
				hashTraits.emplace(type_name, methDecl);
			}
		}
	});

	//free operators
	DeclarationMatcher out_stream_op =
	  functionDecl(
	    unless(hasDeclContext(recordDecl())),
	    matchesName("operator[\\+-\\*\\^\\[\\(\\!\\&\\|\\~\\=\\/\\%\\<\\>]")
	  ).bind("free_operator");
	finder.addMatcher(out_stream_op, this);
	declPrinters.emplace("free_operator", [](DPrinter&, Decl*) {});
	onDeclMatch.emplace("free_operator", [this](Decl const * d)
	{
		if(auto* funcDecl = dyn_cast<FunctionDecl>(d))
		{
			auto getParamTypeName = [](ParmVarDecl const * typeParam)
			{
				QualType canType = typeParam->getType().getCanonicalType()
				                   .getUnqualifiedType().getNonReferenceType();
				canType.removeLocalConst();
				return canType.getAsString();
			};

			if(funcDecl->getNumParams() > 0)
			{
				std::string const left_name = getParamTypeName(funcDecl->getParamDecl(0));
				freeOperator.emplace(left_name, funcDecl);
				if(funcDecl->getNumParams() > 1)
				{
					std::string const right_name = getParamTypeName(funcDecl->getParamDecl(1));
					if(right_name != left_name)
						freeOperatorRight.emplace(right_name, funcDecl);
				}
			}
		}
	});

	// std::exception
	rewriteType(finder, "std::exception", "Throwable");
	rewriteType(finder, "std::logic_error", "Error");
	rewriteType(finder, "std::runtime_error", "Exception");

	methodPrinter("std::exception", "what", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
		{
			if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
			{
				Expr* base = memExpr->isImplicitAccess() ? nullptr : memExpr->getBase();
				pr.TraverseStmt(base);
				pr.stream() << ".";
				pr.stream() << "msg";
			}
		}
	});

	//********************** Containers array and vector ******************************************
	std::string const containers =
	  "^::(boost|std)::(vector|array|set|map|multiset|multimap|unordered_set|unordered_map|"
	  "unordered_multiset|unordered_multimap|queue|stack|list|forcard_list)";

	tmplTypePrinter(finder, "std::map", [](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.addExternInclude("cpp_std", "cpp_std.map");
		printer.stream() << "cpp_std.map!(";
		printer.printTemplateArgument(TSType->getArg(0));
		printer.stream() << ", ";
		printer.printTemplateArgument(TSType->getArg(1));
		printer.stream() << ")";
	});

	finder.addMatcher(callExpr(
	                    anyOf(
	                      callee(functionDecl(matchesName(containers + "\\<.*\\>::size$"))),
	                      callee(memberExpr(
	                               member(matchesName("::size$")),
	                               hasObjectExpression(hasType(namedDecl(matchesName(containers))))
	                             )))
	                  ).bind("std::vector::size"), this);
	stmtPrinters.emplace("std::vector::size", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
			{
				pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
				pr.stream() << ".length";
			}
			else if(auto* impCast = dyn_cast<ImplicitCastExpr>(memCall->getCallee()))
			{
				if(auto* memExpr2 = dyn_cast<MemberExpr>(impCast->getSubExpr()))
				{
					pr.TraverseStmt(memExpr2->isImplicitAccess() ? nullptr : memExpr2->getBase());
					pr.stream() << ".length";
				}
			}
		}
	});


	char const* containerTab[] =
	{
		"std::vector", "std::array", "boost::array", "std::set", "std::map", "std::multiset", "std::multimap",
		"std::unordered_set", "boost::unordered_set", "std::unordered_map", "boost::unordered_map",
		"std::unordered_multiset", "boost::unordered_multiset",
		"std::unordered_multimap", "boost::unordered_multimap",
		"std::queue", "std::stack", "std::list", "std::forcard_list"
	};

	for(char const* container : containerTab)
	{
		for(char const* meth : { "assign", "fill" })
		{
			methodPrinter(container, meth, [](DPrinter & pr, Stmt * s)
			{
				if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
				{
					if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
					{
						pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
						pr.stream() << "[] = ";
						pr.TraverseStmt(*memCall->arg_begin());
					}
				}
			});
		}
	}

	for(char const* container : containerTab)
	{
		methodPrinter(container, "swap", [](DPrinter & pr, Stmt * s)
		{
			if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
			{
				if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
				{
					pr.stream() << "std.algorithm.swap(";
					pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
					pr.stream() << ", ";
					pr.TraverseStmt(*memCall->arg_begin());
					pr.stream() << ')';
					pr.addExternInclude("std.algorithm", "std.algorithm.swap");
				}
			}
		});
	}

	for(char const* container : containerTab)
	{
		for(char const* meth : {"push_back", "emplace_back"})
		{
			methodPrinter(container, meth, [](DPrinter & pr, Stmt * s)
			{
				if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
				{
					if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
					{
						pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
						pr.stream() << ".insertBack(";
						pr.TraverseStmt(*memCall->arg_begin());
						pr.stream() << ')';
					}
				}
			});
		}
	}

	for(char const* container : containerTab)
	{
		methodPrinter(container, "push_front", [](DPrinter & pr, Stmt * s)
		{
			if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
			{
				if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
				{
					pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
					pr.stream() << ".insertFront(";
					pr.TraverseStmt(*memCall->arg_begin());
					pr.stream() << ')';
				}
			}
		});
	}

	operatorCallPrinter(finder, "^::std::basic_string(<|$)", "+=",
	                    [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			pr.TraverseStmt(opCall->getArg(0));
			pr.stream() << " ~= ";
			pr.TraverseStmt(opCall->getArg(1));
		}
	});

	//********************** memory ***************************************************************


	// shared_ptr
	tmplTypePrinter(finder, "std::shared_ptr", [](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		TemplateArgument const& arg = TSType->getArg(0);
		DPrinter::Semantic const sem = DPrinter::getSemantic(arg.getAsType());
		if(sem == DPrinter::Value)
		{
			printer.addExternInclude("std.typecons", "RefCounted");
			printer.stream() << "std.typecons.RefCounted!(";
			printer.printTemplateArgument(TSType->getArg(0));
			printer.stream() << ")";
		}
		else
			printer.printTemplateArgument(TSType->getArg(0));
	});

	globalFuncPrinter(finder, "^::(std|boost)::make_shared", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			TemplateArgument const* tmpArg = getTypeTemplateArgument(call->getCallee(), 0);
			if(tmpArg)
			{
				DPrinter::Semantic const sem = DPrinter::getSemantic(tmpArg->getAsType());
				if(sem != DPrinter::Value)
					pr.stream() << "new ";
				pr.printTemplateArgument(*tmpArg);
				pr.printCallExprArgument(call);
			}
		}
	});

	for(char const * className
	: { "std::__shared_ptr", "std::shared_ptr", "boost::shared_ptr", "std::unique_ptr"})
	{
		methodPrinter(className, "reset", [](DPrinter & pr, Stmt * s)
		{
			if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
			{
				if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
				{
					pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
					pr.stream() << " = ";
					if(memCall->getNumArgs() == 0)
						pr.stream() << "null";
					else
						pr.TraverseStmt(*memCall->arg_begin());
				}
			}
		});
	}

	operatorCallPrinter(finder, "^::std::(__)?shared_ptr(<|$)", "==",
	                    [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = getTypeTemplateArgument(leftOp, 0);
			DPrinter::Semantic const sem = tmpArg ?
			                               DPrinter::getSemantic(tmpArg->getAsType()) :
			                               DPrinter::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == DPrinter::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized == false";
			else
			{
				pr.stream() << " is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	operatorCallPrinter(finder, "^::std::(__)?shared_ptr(<|$)", "!=",
	                    [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = getTypeTemplateArgument(leftOp, 0);
			DPrinter::Semantic const sem = tmpArg ?
			                               DPrinter::getSemantic(tmpArg->getAsType()) :
			                               DPrinter::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == DPrinter::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized";
			else
			{
				pr.stream() << " !is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	// unique_ptr
	tmplTypePrinter(finder, "std::unique_ptr", [](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		TemplateArgument const& arg = TSType->getArg(0);
		DPrinter::Semantic const sem = DPrinter::getSemantic(arg.getAsType());
		if(sem == DPrinter::Value)
		{
			printer.addExternInclude("std.typecons", "RefCounted");
			printer.stream() << "std.typecons.RefCounted!(";
			printer.printTemplateArgument(TSType->getArg(0));
			printer.stream() << ")";
		}
		else
			printer.printTemplateArgument(TSType->getArg(0));
	});

	globalFuncPrinter(finder, "^::(std|boost)::make_unique", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			TemplateArgument const* tmpArg = getTypeTemplateArgument(call->getCallee(), 0);
			if(tmpArg)
			{
				DPrinter::Semantic const sem = DPrinter::getSemantic(tmpArg->getAsType());
				if(sem != DPrinter::Value)
					pr.stream() << "new ";
				pr.printTemplateArgument(*tmpArg);
				pr.printCallExprArgument(call);
			}
		}
	});

	operatorCallPrinter(finder, "^::std::unique_ptr(<|$)", "==", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = getTypeTemplateArgument(leftOp, 0);
			DPrinter::Semantic const sem = tmpArg ?
			                               DPrinter::getSemantic(tmpArg->getAsType()) :
			                               DPrinter::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == DPrinter::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized == false";
			else
			{
				pr.stream() << " is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	operatorCallPrinter(finder, "^::std::unique_ptr(<|$)", "!=", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = getTypeTemplateArgument(leftOp, 0);
			DPrinter::Semantic const sem = tmpArg ?
			                               DPrinter::getSemantic(tmpArg->getAsType()) :
			                               DPrinter::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == DPrinter::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized";
			else
			{
				pr.stream() << " !is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	globalFuncPrinter(finder, "^::std::move", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "cpp_std.move(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ")";
			pr.addExternInclude("cpp_std", "cpp_std.move");
		}
	});

	globalFuncPrinter(finder, "^::std::forward", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
			pr.TraverseStmt(memCall->getArg(0));
	});

	//********************************* std::function *********************************************
	auto function_print = [](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.printTemplateArgument(TSType->getArg(0));
	};
	tmplTypePrinter(finder, "std::function", function_print);
	tmplTypePrinter(finder, "boost::function", function_print);

	//********************** std::stream **********************************************************
	finder.addMatcher(
	  declRefExpr(hasDeclaration(namedDecl(matchesName("cout")))).bind("std::cout"), this);
	stmtPrinters.emplace("std::cout", [](DPrinter & pr, Stmt*)
	{
		pr.stream() << "std.stdio.stdout";
		pr.addExternInclude("std.stdio", "std.stdio.stdout");
	});

	// std::option
	auto optional_to_bool = [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
		{
			if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
			{
				pr.stream() << '!';
				pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
				pr.stream() << ".isNull";
			}
		}
	};
	methodPrinter("std::optional", "operator bool", optional_to_bool);
	methodPrinter("boost::optional", "operator bool", optional_to_bool);

	// *********************************** <utils> **************************
	// pair
	tmplTypePrinter(finder, "std::pair", [](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.addExternInclude("std.typecons", "Tuple");
		printer.stream() << "Tuple!(";
		printer.printTemplateArgument(TSType->getArg(0));
		printer.stream() << ", \"key\", ";
		printer.printTemplateArgument(TSType->getArg(1));
		printer.stream() << ", \"value\")";
	});

	memberPrinter(finder, "^::std::pair\\<.*\\>::second", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".value";
		}
	});

	memberPrinter(finder, "^::std::pair\\<.*\\>::first", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".key";
		}
	});

	globalFuncPrinter(finder, "^::std::swap$", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.swap(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ", ";
			pr.TraverseStmt(memCall->getArg(1));
			pr.stream() << ")";
			pr.addExternInclude("std.algorithm", "std.algorithm.swap");
		}
	});

	globalFuncPrinter(finder, "^::std::max$", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.comparison.max";
			pr.printCallExprArgument(call);
			pr.addExternInclude("std.algorithm.comparison", "std.algorithm.comparison.max");
		}
	});

	//************************** C libraries ******************************************************
	// <stdio>
	finder.addMatcher(
	  declRefExpr(hasDeclaration(functionDecl(hasName("::printf")))).bind("printf"), this);
	stmtPrinters.emplace("printf", [](DPrinter & printer, Stmt*)
	{
		printer.addExternInclude("std.stdio", "writef");
		printer.stream() << "writef";
	});

	// <string>
	globalFuncPrinter(finder, "^(::std)?::strlen", [](DPrinter & pr, Stmt * s)
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
	cFuncPrinter(finder, "stdlib", "rand");

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
		cFuncPrinter(finder, "math", func);

	// <ctime>
	cFuncPrinter(finder, "time", "time");

	//<assert>
	globalFuncPrinter(finder, "^(::std)?::_wassert", [](DPrinter&, Stmt*) {});

	//**************************** boost **********************************************************

	//BOOST_THROW_EXCEPTION
	globalFuncPrinter(finder, "throw_exception_", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "throw ";
			pr.TraverseStmt(*memCall->arg_begin());
		}
	});

	//boost::serialisation
	rewriteType(finder, "boost::serialization::access", "int");

	//boost::log //hasDeclaration(varDecl(matchesName("^_boost_log_record_\\d+$")))
	finder.addMatcher(
	  forStmt(
	    hasLoopInit(declStmt(hasSingleDecl(varDecl(hasType(namedDecl(matchesName("record"))))))))
	  .bind("boost::log"),
	  this);
	stmtPrinters.emplace("boost::log", [](DPrinter & pr, Stmt * s)
	{
		if(auto* forSt = dyn_cast<ForStmt>(s))
		{
			pr.TraverseStmt(forSt->getBody());
			pr.stream() << ';';
		}
	});

	return finder;
}

void MatchContainer::run(const ast_matchers::MatchFinder::MatchResult& Result)
{
	// To call printers during the D print
	for(auto const& tag_n_func : typePrinters)
	{
		if(auto* t = Result.Nodes.getNodeAs<Type>(tag_n_func.first))
			typeTags.emplace(t, tag_n_func.first);
	}
	for(auto const& tag_n_func : stmtPrinters)
	{
		if(auto* s = Result.Nodes.getNodeAs<Stmt>(tag_n_func.first))
			stmtTags.emplace(s, tag_n_func.first);
	}
	for(auto const& tag_n_func : declPrinters)
	{
		if(auto* d = Result.Nodes.getNodeAs<Decl>(tag_n_func.first))
			declTags.emplace(d, tag_n_func.first);
	}

	// To call a special handling now
	for(auto const& name_n_func : onDeclMatch)
	{
		if(auto* d = Result.Nodes.getNodeAs<Decl>(name_n_func.first))
			name_n_func.second(d);
	}
	for(auto const& name_n_func : onStmtMatch)
	{
		if(auto* s = Result.Nodes.getNodeAs<Stmt>(name_n_func.first))
			name_n_func.second(s);
	}
	for(auto const& name_n_func : onTypeMatch)
	{
		if(auto* t = Result.Nodes.getNodeAs<Type>(name_n_func.first))
			name_n_func.second(t);
	}
}


std::function<void(DPrinter& printer, clang::Stmt*)>
MatchContainer::getPrinter(clang::Stmt const* node) const
{
	auto iter_pair = stmtTags.equal_range(node);
	if(iter_pair.first != iter_pair.second)
	{
		auto iter2 = stmtPrinters.find(iter_pair.first->second);
		if(iter2 != stmtPrinters.end())
			return iter2->second;
	}

	return std::function<void(DPrinter& printer, clang::Stmt const*)>();
}

std::function<void(DPrinter& printer, clang::Decl*)>
MatchContainer::getPrinter(clang::Decl const* node) const
{
	auto iter_pair = declTags.equal_range(node);
	if(iter_pair.first != iter_pair.second)
	{
		auto iter2 = declPrinters.find(iter_pair.first->second);
		if(iter2 != declPrinters.end())
			return iter2->second;
	}

	return std::function<void(DPrinter& printer, clang::Decl const*)>();
}

std::function<void(DPrinter& printer, clang::Type*)>
MatchContainer::getPrinter(clang::Type const* node) const
{
	auto iter_pair = typeTags.equal_range(node);
	if(iter_pair.first != iter_pair.second)
	{
		auto iter2 = typePrinters.find(iter_pair.first->second);
		if(iter2 != typePrinters.end())
			return iter2->second;
	}

	return std::function<void(DPrinter& printer, clang::Type const*)>();
}
