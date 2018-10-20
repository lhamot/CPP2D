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
#include "CustomPrinters.h"
#include "Spliter.h"



using namespace clang;
using namespace clang::ast_matchers;

TemplateArgument const* MatchContainer::getTemplateTypeArgument(Expr const* e, size_t tmplArgIndex)
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
						assert(TSType->getNumArgs() > tmplArgIndex);
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
                                 std::string const& newName,
                                 std::string const& newImport)
{
	using namespace clang::ast_matchers;
	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName(oldName))))
	                  .bind(oldName), this);
	typePrinters.emplace(oldName, [newName, newImport](DPrinter & pr, Type*)
	{
		pr.stream() << newName;
		if(not newImport.empty())
			pr.addExternInclude(newImport, newName);
	});
};

void MatchContainer::methodPrinter(std::string const& className,
                                   std::string const& methodName,
                                   StmtPrinter const&  printer)
{
	methodPrinters[methodName].emplace(className, printer);
};

void MatchContainer::globalFuncPrinter(
  std::string const& funcName,
  StmtPrinter const& printer
)
{
	globalFuncPrinters[funcName] = printer;
};

void MatchContainer::tmplTypePrinter(std::string const& name,
                                     DeclPrinter const& printMapDecl)
{
	customTypePrinters.emplace(name, printMapDecl);
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
  std::string const& lib,
  std::string const& func)
{
	globalFuncPrinter("^(::std)?::" + func + "(<|$)", [lib, func](DPrinter & pr, Stmt * s)
	{
		if(auto* call = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc." << lib << "." << func;
			pr.stream() << "(";
			Spliter spliter(pr, ", ");
			for(Expr* arg : call->arguments())
			{
				if(arg->getStmtClass() == Stmt::StmtClass::CXXDefaultArgExprClass)
					break;
				spliter.split();

				std::string const argType = arg->getType().getAsString();
				bool const passCString = (argType == "const char *" || argType == "char *");
				if(passCString)
				{
					pr.addExternInclude("std.string", "std.string.toStringz");
					pr.stream() << "std.string.toStringz(";
				}
				pr.TraverseStmt(arg);
				if(passCString)
					pr.stream() << ")";
			}
			pr.stream() << ")";

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

	for(auto printerRegisterers : CustomPrinters::getInstance().getRegisterers())
		printerRegisterers(*this, finder);

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
