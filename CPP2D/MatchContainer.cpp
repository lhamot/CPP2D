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


clang::ast_matchers::MatchFinder MatchContainer::getMatcher()
{
	using namespace clang::ast_matchers;
	MatchFinder finder;

	// Some debug bind slot
	onStmtMatch.emplace("dump", [this](Stmt const * d) {d->dump(); });
	onTypeMatch.emplace("dump", [this](Type const * d) {d->dump(); });
	onDeclMatch.emplace("dump", [this](Decl const * d) {d->dump(); });
	onDeclMatch.emplace("print_name", [this](Decl const * d)
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
	declPrinters.emplace("dont_print_this_decl", [this](DPrinter&, Decl*) {});
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
	declPrinters.emplace("free_operator", [this](DPrinter&, Decl*) {});
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

	// printf
	finder.addMatcher(
	  declRefExpr(hasDeclaration(functionDecl(hasName("::printf")))).bind("printf"), this);
	stmtPrinters.emplace("printf", [this](DPrinter & printer, Stmt*)
	{
		printer.addExternInclude("std.stdio", "writef");
		printer.stream() << "writef";
	});

	auto rewriteType = [this](clang::ast_matchers::MatchFinder & finder,
	                          std::string const & oldName,
	                          std::string const & newName)
	{
		using namespace clang::ast_matchers;
		finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName(oldName))))
		                  .bind(oldName), this);
		typePrinters.emplace(oldName,
		[newName](DPrinter & pr, Type*) {pr.stream() << newName; });
	};

	auto methodPrinter = [this](clang::ast_matchers::MatchFinder & finder,
	                            std::string const & classRegexpr,
	                            std::string const & methodRegexpr,
	                            std::string const & tag,
	                            auto && printer)
	{
		finder.addMatcher(cxxMemberCallExpr(
		                    thisPointerType(cxxRecordDecl(isSameOrDerivedFrom(matchesName(classRegexpr)))),
		                    callee(cxxMethodDecl(matchesName(methodRegexpr)))
		                  ).bind(tag), this);
		stmtPrinters.emplace(tag, printer);
	};

	auto globalFuncPrinter = [this](clang::ast_matchers::MatchFinder & finder,
	                                std::string const & regexpr,
	                                std::string const & tag,
	                                auto && printer)
	{
		finder.addMatcher(callExpr(callee(functionDecl(matchesName(regexpr)))).bind(tag), this);
		finder.addMatcher(
		  callExpr(callee(unresolvedLookupExpr(uleMatchesName(regexpr)))).bind(tag), this);
		stmtPrinters.emplace(tag, printer);
	};

	// std::exception
	rewriteType(finder, "std::exception", "Throwable");
	rewriteType(finder, "std::logic_error", "Error");
	rewriteType(finder, "std::runtime_error", "Exception");

	methodPrinter(finder, "^::std::exception$", "::what$", "std::exception::what",
	              [this](DPrinter & pr, Stmt * s)
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

	auto tmplTypePrinter = [this](clang::ast_matchers::MatchFinder & finder,
	                              std::string const & name,
	                              auto && printer)
	{
		finder.addMatcher(
		  templateSpecializationType(hasDeclaration(namedDecl(hasName(name)))).bind(name),
		  this);
		typePrinters.emplace(name, printer);
	};

	tmplTypePrinter(finder, "std::map", [this](DPrinter & printer, Type * Type)
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
	stmtPrinters.emplace("std::vector::size", [this](DPrinter & pr, Stmt * s)
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

	methodPrinter(finder, containers, "::(assign|fill)$", "std::vector::assign",
	              [this](DPrinter & pr, Stmt * s)
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

	methodPrinter(finder, containers + "\\<.*\\>", "::swap$", "std::vector::swap",
	              [this](DPrinter & pr, Stmt * s)
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

	methodPrinter(finder, containers + "\\<.*\\>", "::push_back$", "std::vector::push_back",
	              [this](DPrinter & pr, Stmt * s)
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

	methodPrinter(finder, containers + "\\<.*\\>", "::push_front$", "std::vector::push_back",
	              [this](DPrinter & pr, Stmt * s)
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

	finder.addMatcher(cxxOperatorCallExpr(
	                    hasArgument(0, hasType(cxxRecordDecl(isSameOrDerivedFrom(matchesName("^::std::basic_string\\<.*\\>"))))),
	                    hasOverloadedOperatorName("+=")
	                  ).bind("std::string::operator +="), this);
	stmtPrinters.emplace("std::string::operator +=", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* opCall = dyn_cast<CXXOperatorCallExpr>(s))
		{
			pr.TraverseStmt(opCall->getArg(0));
			pr.stream() << " ~= ";
			pr.TraverseStmt(opCall->getArg(1));
		}
	});

	//********************** memory ***************************************************************

	tmplTypePrinter(finder, "std::shared_ptr", [this](DPrinter & printer, Type * Type)
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

	globalFuncPrinter(finder, "^::(std|boost)::make_shared", "std::make_shared", [this](DPrinter & pr, Stmt * s)
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

	methodPrinter(finder, "^::(std|boost)::shared_ptr\\<.*\\>", "::reset$", "std::shared_ptr::reset$",
	              [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CXXMemberCallExpr>(s))
		{
			if(auto* memExpr = dyn_cast<MemberExpr>(memCall->getCallee()))
			{
				pr.TraverseStmt(memExpr->isImplicitAccess() ? nullptr : memExpr->getBase());
				pr.stream() << " = ";
				pr.TraverseStmt(*memCall->arg_begin());
			}
		}
	});

	finder.addMatcher(cxxOperatorCallExpr(
	                    hasArgument(0, hasType(cxxRecordDecl(isSameOrDerivedFrom(matchesName("^::std::shared_ptr\\<.*\\>"))))),
	                    hasOverloadedOperatorName("==")
	                  ).bind("std::shared_ptr::operator=="), this);
	stmtPrinters.emplace("std::shared_ptr::operator==", [this](DPrinter & pr, Stmt * s)
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

	finder.addMatcher(cxxOperatorCallExpr(
	                    hasArgument(0, hasType(cxxRecordDecl(isSameOrDerivedFrom(matchesName("^::std::shared_ptr\\<.*\\>"))))),
	                    hasOverloadedOperatorName("!=")
	                  ).bind("std::shared_ptr::operator!="), this);
	stmtPrinters.emplace("std::shared_ptr::operator!=", [this](DPrinter & pr, Stmt * s)
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


	//********************** std::stream **********************************************************
	finder.addMatcher(
	  declRefExpr(hasDeclaration(namedDecl(matchesName("cout")))).bind("std::cout"), this);
	stmtPrinters.emplace("std::cout", [this](DPrinter & pr, Stmt*)
	{
		pr.stream() << "std.stdio.stdout";
		pr.addExternInclude("std.stdio", "std.stdio.stdout");
	});

	// std::option
	methodPrinter(finder,
	              "^::(std|boost)::optional\\<.*\\>", "::operator bool$",
	              "std::optional::operator bool",
	              [this](DPrinter & pr, Stmt * s)
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
	});


	// *********************************** <utils> **************************
	// pair
	tmplTypePrinter(finder, "std::pair", [this](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.addExternInclude("std.typecons", "Tuple");
		printer.stream() << "Tuple!(";
		printer.printTemplateArgument(TSType->getArg(0));
		printer.stream() << ", \"key\", ";
		printer.printTemplateArgument(TSType->getArg(1));
		printer.stream() << ", \"value\")";
	});

	auto memberPrinter = [this](clang::ast_matchers::MatchFinder & finder,
	                            std::string const & regexpr,
	                            std::string const & tag,
	                            auto && printer)
	{
		finder.addMatcher(memberExpr(member(matchesName(regexpr))).bind(tag), this);
		stmtPrinters.emplace(tag, printer);
	};

	memberPrinter(finder,
	              "^::std::pair\\<.*\\>::second",
	              "std::pair::second",
	              [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".value";
		}
	});

	memberPrinter(finder,
	              "^::std::pair\\<.*\\>::first",
	              "std::pair::first",
	              [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memExpr = dyn_cast<MemberExpr>(s))
		{
			Expr* base = memExpr->getBase();
			pr.TraverseStmt(base);
			pr.stream() << ".key";
		}
	});

	globalFuncPrinter(finder, "^::std::swap$", "std::swap", [this](DPrinter & pr, Stmt * s)
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

	globalFuncPrinter(finder, "^::std::max$", "std::max", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.comparison.max";
			pr.printCallExprArgument(call);
			pr.addExternInclude("std.algorithm.comparison", "std.algorithm.comparison.max");
		}
	});

	globalFuncPrinter(finder, "^(::std)?::strlen", "std::strlen", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc.string.strlen(";
			pr.TraverseStmt(call->getArg(0));
			pr.stream() << ".ptr)";
			pr.addExternInclude("core.stdc.string", "core.stdc.string.strlen");
		}
	});

	globalFuncPrinter(finder, "^(::std)?::rand", "std::rand", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc.stdlib.rand";
			pr.printCallExprArgument(call);
			pr.addExternInclude("core.stdc.stdlib", "core.stdc.stdlib.rand");
		}
	});

	// <ctime>
	globalFuncPrinter(finder, "^(::std)?::time$", "std::time", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc.time.time";
			pr.printCallExprArgument(call);
			pr.addExternInclude("core.stdc.time", "core.stdc.time.time");
		}
	});

	//<assert>
	globalFuncPrinter(finder, "^(::std)?::_wassert", "_wassert", [this](DPrinter&, Stmt*) {});

	//BOOST_THROW_EXCEPTION
	globalFuncPrinter(
	  finder, "throw_exception_", "throw_exception_", [this](DPrinter & pr, Stmt * s)
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
	stmtPrinters.emplace("boost::log", [this](DPrinter & pr, Stmt * s)
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
		if(auto* type = Result.Nodes.getNodeAs<Type>(tag_n_func.first))
			typeTags.emplace(type, tag_n_func.first);
	}
	for(auto const& tag_n_func : stmtPrinters)
	{
		if(auto* stmt = Result.Nodes.getNodeAs<Stmt>(tag_n_func.first))
			stmtTags.emplace(stmt, tag_n_func.first);
	}
	for(auto const& tag_n_func : declPrinters)
	{
		if(auto* decl = Result.Nodes.getNodeAs<Decl>(tag_n_func.first))
			declTags.emplace(decl, tag_n_func.first);
	}

	// To call a special handling now
	for(auto const& name_n_func : onDeclMatch)
	{
		if(auto* decl = Result.Nodes.getNodeAs<Decl>(name_n_func.first))
			name_n_func.second(decl);
	}
	for(auto const& name_n_func : onStmtMatch)
	{
		if(auto* stmt = Result.Nodes.getNodeAs<Stmt>(name_n_func.first))
			name_n_func.second(stmt);
	}
	for(auto const& name_n_func : onTypeMatch)
	{
		if(auto* type = Result.Nodes.getNodeAs<Type>(name_n_func.first))
			name_n_func.second(type);
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
