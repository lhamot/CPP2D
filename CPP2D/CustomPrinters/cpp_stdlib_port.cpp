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
#include "../Spliter.h"
#include "../Options.h"

using namespace clang;
using namespace clang::ast_matchers;

//! @todo This function has to be split across all includes
void cpp_stdlib_port(MatchContainer& mc, MatchFinder& finder)
{
	// ********************************* <exception> **********************************************
	mc.rewriteType(finder, "std::exception", "Throwable", "");
	mc.rewriteType(finder, "std::logic_error", "Error", "");
	mc.rewriteType(finder, "std::runtime_error", "Exception", "");
	mc.rewriteType(finder, "std::out_of_range", "core.exception.RangeError", "core.exception");
	mc.rewriteType(finder, "std::bad_alloc", "core.exception.OutOfMemoryError", "core.exception");

	mc.methodPrinter("std::exception", "what", [](DPrinter & pr, Stmt * s)
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

	mc.tmplTypePrinter("^::std::map($|<.*>$)", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			printer.addExternInclude("cpp_std", "cpp_std.map");
			printer.stream() << "cpp_std.map!(";
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			printer.printTemplateArgument(tmpArgsSpec.get(0));
			printer.stream() << ", ";
			printer.printTemplateArgument(tmpArgsSpec.get(1));
			printer.stream() << ")";
		}
	});

	mc.tmplTypePrinter("^::std::map<.*>::value_type$", [](DPrinter & printer, Decl * d) //std::map<.*>::
	{
		if(auto* named = dyn_cast<NamedDecl>(d))
		{
			if(DeclContext* ctx = named->getDeclContext())
			{
				if(auto* tmpSpec = dyn_cast<ClassTemplateSpecializationDecl>(ctx))
				{
					printer.stream() << "cpp_std.pair!(";
					TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
					printer.printTemplateArgument(tmpArgsSpec.get(0));
					printer.stream() << ", ";
					printer.printTemplateArgument(tmpArgsSpec.get(1));
					printer.stream() << ")";

				}
			}
		}
	});

	finder.addMatcher(callExpr(
	                    anyOf(
	                      callee(functionDecl(matchesName(containers + "\\<.*\\>::size$"))),
	                      callee(memberExpr(
	                               member(matchesName("::size$")),
	                               hasObjectExpression(hasType(namedDecl(matchesName(containers))))
	                             )))
	                  ).bind("std::vector::size"), &mc);
	mc.stmtPrinters.emplace("std::vector::size", [](DPrinter & pr, Stmt * s)
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
			mc.methodPrinter(container, meth, [](DPrinter & pr, Stmt * s)
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
		mc.methodPrinter(container, "swap", [](DPrinter & pr, Stmt * s)
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
		for(char const* meth : { "push_back", "emplace_back" })
		{
			mc.methodPrinter(container, meth, [](DPrinter & pr, Stmt * s)
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
		mc.methodPrinter(container, "push_front", [](DPrinter & pr, Stmt * s)
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

	for(char const* container : containerTab)
	{
		mc.methodPrinter(container, "at", [](DPrinter & pr, Stmt * s)
		{
			if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
			{
				if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
				{
					pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
					pr.stream() << '[';
					pr.TraverseStmt(*memCall->arg_begin());
					pr.stream() << ']';
				}
			}
		});
	}

	mc.operatorCallPrinter(finder, "^::std::(__cxx11::)?(basic_)?string(<|$)", "+=",
	                       [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			pr.TraverseStmt(opCall->getArg(0));
			pr.stream() << " ~= ";
			pr.TraverseStmt(opCall->getArg(1));
		}
	});

	//************************************ <memory> ***********************************************


	// shared_ptr
	mc.tmplTypePrinter("^::std::shared_ptr", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			TemplateArgument const& arg = tmpArgsSpec.get(0);
			TypeOptions::Semantic const sem = DPrinter::getSemantic(arg.getAsType());
			if(sem == TypeOptions::Value)
			{
				printer.addExternInclude("std.typecons", "RefCounted");
				printer.stream() << "std.typecons.RefCounted!(";
				printer.printTemplateArgument(arg);
				printer.stream() << ")";
			}
			else
				printer.printTemplateArgument(arg);
		}
	});

	finder.addMatcher(implicitCastExpr(
	                    castExpr(hasCastKind(CK_ConstructorConversion)),
	                    hasImplicitDestinationType(hasDeclaration(namedDecl(matchesName("^::std::(__)?shared_ptr(<|$)"))))
	                  ).bind("shared_ptr_implicit_cast"), &mc);
	mc.stmtPrinters.emplace("shared_ptr_implicit_cast", [](DPrinter & pr, Stmt * s)
	{
		if(auto* cast = dyn_cast<ImplicitCastExpr>(s))
		{
			pr.TraverseStmt(cast->getSubExpr());
		}
	});


	mc.globalFuncPrinter("^::(std|boost)::make_shared(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(call->getCallee(), 0);
			if(tmpArg)
			{
				TypeOptions::Semantic const sem = DPrinter::getSemantic(tmpArg->getAsType());
				if(sem != TypeOptions::Value)
				{
					pr.stream() << "new ";
					pr.printTemplateArgument(*tmpArg);
					pr.printCallExprArgument(call);
				}
				else
				{
					pr.stream() << "std.typecons.RefCounted!(";
					pr.printTemplateArgument(*tmpArg);
					pr.stream() << ")(";
					pr.printTemplateArgument(*tmpArg);
					pr.printCallExprArgument(call);
					pr.stream() << ")";
				}
			}
		}
	});

	for(char const * className
	: { "std::__shared_ptr", "std::shared_ptr", "boost::shared_ptr", "std::unique_ptr" })
	{
		mc.methodPrinter(className, "reset", [](DPrinter & pr, Stmt * s)
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

	mc.operatorCallPrinter(finder, "^::std::(__)?shared_ptr(<|$)", "==",
	                       [&mc](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(leftOp, 0);
			TypeOptions::Semantic const sem = tmpArg ?
			                                  DPrinter::getSemantic(tmpArg->getAsType()) :
			                                  TypeOptions::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == TypeOptions::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized == false";
			else
			{
				pr.stream() << " is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	mc.operatorCallPrinter(finder, "^::std::(__)?(shared_ptr|unique_ptr)(<|$)", "*",
	                       [&mc](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
			pr.TraverseStmt(opCall->getArg(0));
	});

	mc.operatorCallPrinter(finder, "^::std::(__)?shared_ptr(<|$)", "!=",
	                       [&mc](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(leftOp, 0);
			TypeOptions::Semantic const sem = tmpArg ?
			                                  DPrinter::getSemantic(tmpArg->getAsType()) :
			                                  TypeOptions::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == TypeOptions::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized";
			else
			{
				pr.stream() << " !is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	// unique_ptr
	mc.tmplTypePrinter("^::std::unique_ptr", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			TemplateArgument const& arg = tmpArgsSpec.get(0);
			TypeOptions::Semantic const sem = DPrinter::getSemantic(arg.getAsType());
			if(sem == TypeOptions::Value)
			{
				printer.addExternInclude("std.typecons", "RefCounted");
				printer.stream() << "std.typecons.RefCounted!(";
				printer.printTemplateArgument(arg);
				printer.stream() << ")";
			}
			else
				printer.printTemplateArgument(arg);
		}
	});

	mc.globalFuncPrinter("^::(std|boost)::make_unique(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(call->getCallee(), 0);
			if(tmpArg)
			{
				TypeOptions::Semantic const sem = DPrinter::getSemantic(tmpArg->getAsType());
				if(sem != TypeOptions::Value)
					pr.stream() << "new ";
				pr.printTemplateArgument(*tmpArg);
				pr.printCallExprArgument(call);
			}
		}
	});

	mc.operatorCallPrinter(finder, "^::std::unique_ptr(<|$)", "==", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(leftOp, 0);
			TypeOptions::Semantic const sem = tmpArg ?
			                                  DPrinter::getSemantic(tmpArg->getAsType()) :
			                                  TypeOptions::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == TypeOptions::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized == false";
			else
			{
				pr.stream() << " is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	mc.operatorCallPrinter(finder, "^::std::unique_ptr(<|$)", "!=", [](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			Expr* leftOp = opCall->getArg(0);
			TemplateArgument const* tmpArg = MatchContainer::getTemplateTypeArgument(leftOp, 0);
			TypeOptions::Semantic const sem = tmpArg ?
			                                  DPrinter::getSemantic(tmpArg->getAsType()) :
			                                  TypeOptions::Reference;

			pr.TraverseStmt(leftOp);
			Expr* rightOp = opCall->getArg(1);
			if(sem == TypeOptions::Value and isa<CXXNullPtrLiteralExpr>(rightOp))
				pr.stream() << ".refCountedStore.isInitialized";
			else
			{
				pr.stream() << " !is ";
				pr.TraverseStmt(rightOp);
			}
		}
	});

	// ******************************* <functional> ***********************************************

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
	finder.addMatcher(hash_trait, &mc);
	mc.declPrinters.emplace("dont_print_this_decl", [](DPrinter&, Decl*) {});
	mc.onDeclMatch.emplace("hash_method", [&mc](Decl const * d)
	{
		if(auto* methDecl = dyn_cast<CXXMethodDecl>(d))
		{
			auto* tmplClass = dyn_cast<ClassTemplateSpecializationDecl>(methDecl->getParent());
			TemplateArgumentList const& tmpArgs = tmplClass->getTemplateArgs();
			if(tmpArgs.size() == 1)
			{
				auto const type_name = tmpArgs[0].getAsType().getCanonicalType().getAsString();
				mc.hashTraits.emplace(type_name, methDecl);
			}
		}
	});

	// std::function
	auto function_print = [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			printer.printTemplateArgument(tmpArgsSpec.get(0));
		}
	};
	mc.tmplTypePrinter("^::std::function", function_print);
	mc.tmplTypePrinter("^::boost::function", function_print);

	// ********************** <iostream> **********************************************************
	// std::cout
	finder.addMatcher(
	  declRefExpr(hasDeclaration(namedDecl(matchesName("cout")))).bind("std::cout"), &mc);
	mc.stmtPrinters.emplace("std::cout", [](DPrinter & pr, Stmt*)
	{
		pr.stream() << "OStream(std.stdio.stdout)";
		pr.addExternInclude("std.stdio", "std.stdio.stdout");
		pr.addExternInclude("cpp_std", "OStream");
	});

	// std::endl
	finder.addMatcher(
	  implicitCastExpr(hasSourceExpression(declRefExpr(hasDeclaration(namedDecl(matchesName("endl")))))).bind("std::endl"), &mc);
	mc.stmtPrinters.emplace("std::endl", [](DPrinter & pr, Stmt*)
	{
		pr.stream() << "\"\\n\"";
	});

	Options::getInstance().types["class std::basic_stringstream<"].semantic = TypeOptions::Value;
	Options::getInstance().types["class std::__cxx11::basic_stringstream<"].semantic = TypeOptions::Value;
	mc.tmplTypePrinter("^::std::(__cxx11::)?stringstream", [](DPrinter & printer, Decl*)
	{
		printer.addExternInclude("cpp_std", "cpp_std.Stringstream");
		printer.stream() << "cpp_std.Stringstream";
	});

	mc.tmplTypePrinter("^::std::ios_base", [](DPrinter & printer, Decl*)
	{
		printer.addExternInclude("cpp_std", "cpp_std.ios_base");
		printer.stream() << "cpp_std.ios_base";
	});

	// ********************** <optional> **********************************************************
	// std::optional
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
	mc.methodPrinter("std::optional", "operator bool", optional_to_bool);
	mc.methodPrinter("boost::optional", "operator bool", optional_to_bool);

	// ************************************** <utility> *******************************************

	mc.globalFuncPrinter("^::std::move(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "cpp_std.move(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ")";
			pr.addExternInclude("cpp_std", "cpp_std.move");
		}
	});

	mc.globalFuncPrinter("^::std::forward(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
			pr.TraverseStmt(memCall->getArg(0));
	});


	// pair
	mc.tmplTypePrinter("^::std::pair", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			printer.addExternInclude("std.typecons", "Tuple");
			printer.stream() << "Tuple!(";
			printer.printTemplateArgument(tmpArgsSpec.get(0));
			printer.stream() << ", \"key\", ";
			printer.printTemplateArgument(tmpArgsSpec.get(1));
			printer.stream() << ", \"value\")";
		}
	});

	mc.memberPrinter(finder, "^::std::pair\\<.*\\>::second", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".value";
		}
	});

	mc.memberPrinter(finder, "^::std::pair\\<.*\\>::first", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".key";
		}
	});

	mc.globalFuncPrinter("^::std::swap(<|$)", [](DPrinter & pr, Stmt * s)
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

	mc.globalFuncPrinter("^::std::make_pair(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "cpp_std.make_pair(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ", ";
			pr.TraverseStmt(memCall->getArg(1));
			pr.stream() << ")";
			pr.addExternInclude("cpp_std", "cpp_std.make_pair");
		}
	});

	mc.globalFuncPrinter("^::std::get(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << "[";
			TemplateArgument const* index =
			  MatchContainer::getTemplateTypeArgument(memCall->getCallee(), 0);
			if(index)
				pr.printTemplateArgument(*index);
			else
				pr.stream() << "<tmpl arg is null!>";
			pr.stream() << "]";
		}
	});

	// ************************************ <tuple> *******************************************
	// tuple
	mc.tmplTypePrinter("^::std::tuple", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			printer.addExternInclude("std.typecons", "Tuple");
			printer.stream() << "Tuple!(";
			Spliter spliter(printer, ", ");
			for(unsigned int arg_idx = 0; arg_idx < tmpArgsSpec.size(); ++arg_idx)
			{
				spliter.split();
				printer.printTemplateArgument(tmpArgsSpec.get(arg_idx));
			}
			printer.stream() << ")";
		}
	});
	Options::getInstance().types["class std::tuple<"].semantic = TypeOptions::Value;

	// ************************************ <algorithm> *******************************************
	mc.globalFuncPrinter("^::std::max(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.comparison.max";
			pr.printCallExprArgument(call);
			pr.addExternInclude("std.algorithm.comparison", "std.algorithm.comparison.max");
		}
	});

	mc.globalFuncPrinter("^::std::min(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.comparison.min";
			pr.printCallExprArgument(call);
			pr.addExternInclude("std.algorithm", "std.algorithm.comparison.min");
		}
	});

	// ************************************ <string> *******************************************
	mc.globalFuncPrinter("^::std::to_string(<|$)", [](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.conv.to!string";
			pr.printCallExprArgument(call);
			pr.addExternInclude("std.conv", "std.conv.to!string");
		}
	});

	mc.methodPrinter("std::basic_string", "c_str", [](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
		{
			if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
				pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
		}
	});

	mc.tmplTypePrinter("^::std::(__cxx11::)?basic_string", [](DPrinter & printer, Decl * d)
	{
		if(auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(d))
		{
			TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
			printer.stream() << "immutable(";
			printer.printTemplateArgument(tmpArgsSpec.get(0));
			printer.stream() << ")[]";
		}
	});
}

REG_CUSTOM_PRINTER(cpp_stdlib_port);
