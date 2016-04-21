#include "MatchContainer.h"

#include "DPrinter.h"

#include <iostream>

using namespace clang;

clang::ast_matchers::MatchFinder MatchContainer::getMatcher()
{
	using namespace clang::ast_matchers;
	MatchFinder finder;

	// Some debug bind slot
	on_stmt_match.emplace("dump", [this](Stmt const * d) {d->dump(); });
	on_type_match.emplace("dump", [this](Type const * d) {d->dump(); });
	on_decl_match.emplace("dump", [this](Decl const * d) {d->dump(); });
	on_decl_match.emplace("print_name", [this](Decl const * d)
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
	on_decl_match.emplace("hash_method", [this](Decl const * d)
	{
		if(auto* methDecl = dyn_cast<CXXMethodDecl>(d))
		{
			auto* tmplClass = dyn_cast<ClassTemplateSpecializationDecl>(methDecl->getParent());
			TemplateArgumentList const& tmpArgs = tmplClass->getTemplateArgs();
			if(tmpArgs.size() == 1)
			{
				auto const type_name = tmpArgs[0].getAsType().getCanonicalType().getAsString();
				hash_traits.emplace(type_name, methDecl);
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
	on_decl_match.emplace("free_operator", [this](Decl const * d)
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
				free_operator.emplace(left_name, funcDecl);
				if(funcDecl->getNumParams() > 1)
				{
					std::string const right_name = getParamTypeName(funcDecl->getParamDecl(1));
					if(right_name != left_name)
						free_operator_right.emplace(right_name, funcDecl);
				}
			}
		}
	});

	// printf
	finder.addMatcher(declRefExpr(hasDeclaration(functionDecl(hasName("::printf")))).bind("printf"), this);
	stmtPrinters.emplace("printf", [this](DPrinter & printer, Stmt*)
	{
		printer.addExternInclude("std.stdio");
		printer.stream() << "writef";
	});

	// pair
	finder.addMatcher(templateSpecializationType(hasDeclaration(namedDecl(hasName("std::pair")))).bind("std::pair"), this);
	typePrinters.emplace("std::pair", [this](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.addExternInclude("std.typecons");
		printer.stream() << "Tuple!(";
		printer.PrintTemplateArgument(TSType->getArg(0));
		printer.stream() << ", \"first\", ";
		printer.PrintTemplateArgument(TSType->getArg(1));
		printer.stream() << ", \"second\")";
	});

	// std::exception
	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName("std::exception")))).bind("std::exception"), this);
	typePrinters.emplace("std::exception", [this](DPrinter & pr, Type*) {pr.stream() << "Throwable"; pr.addExternInclude("object"); });

	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName("std::logic_error")))).bind("std::logic_error"), this);
	typePrinters.emplace("std::logic_error", [this](DPrinter & pr, Type*) {pr.stream() << "Error"; pr.addExternInclude("object"); });

	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName("std::runtime_error")))).bind("std::runtime_error"), this);
	typePrinters.emplace("std::runtime_error", [this](DPrinter & pr, Type*) {pr.stream() << "Exception"; pr.addExternInclude("object"); });

	finder.addMatcher(cxxMemberCallExpr(thisPointerType(namedDecl(hasName("std::exception")))).bind("std::exception::what"), this);
	stmtPrinters.emplace("std::exception::what", [this](DPrinter & pr, Stmt * s)
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
	std::string const containers = "^::(boost|std)::(vector|array|set|map|multiset|multimap"
	                               "|unordered_set|unordered_map|unordered_multiset|unordered_multimap|queue|stack|list"
	                               "|forcard_list)";

	finder.addMatcher(templateSpecializationType(hasDeclaration(namedDecl(hasName("std::map")))).bind("std::map"), this);
	typePrinters.emplace("std::map", [this](DPrinter & printer, Type * Type)
	{
		auto* TSType = dyn_cast<TemplateSpecializationType>(Type);
		printer.addExternInclude("cpp_std");
		printer.stream() << "cpp_std.map!(";
		printer.PrintTemplateArgument(TSType->getArg(0));
		printer.stream() << ", ";
		printer.PrintTemplateArgument(TSType->getArg(1));
		printer.stream() << ")";
	});

	finder.addMatcher(callExpr(
	                    anyOf(
	                      callee(functionDecl(matchesName(containers + "\\<.*\\>::size$"))),
	                      callee(memberExpr(
	                               member(matchesName("::size$")),
	                               hasObjectExpression(hasType(namedDecl(matchesName(containers))))))
	                    )
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

	finder.addMatcher(cxxMemberCallExpr(
	                    anyOf(
	                      callee(cxxMethodDecl(matchesName(containers + "\\<.*\\>::fill$"))),
	                      callee(cxxMethodDecl(matchesName(containers + "\\<.*\\>::assign")))
	                    )).bind("std::array::fill"), this);
	stmtPrinters.emplace("std::array::fill", [this](DPrinter & pr, Stmt * s)
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

	finder.addMatcher(cxxMemberCallExpr(callee(cxxMethodDecl(matchesName(containers + "\\<.*\\>::swap$")))
	                                   ).bind("std::vector::swap"), this);
	stmtPrinters.emplace("std::vector::swap", [this](DPrinter & pr, Stmt * s)
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
				pr.addExternInclude("std.algorithm");
			}
		}
	});

	// std::option
	finder.addMatcher(cxxMemberCallExpr(callee(cxxMethodDecl(matchesName("^::(std|boost)::optional\\<.*\\>::operator bool")))
	                                   ).bind("std::optional::operator bool"), this);
	stmtPrinters.emplace("std::optional::operator bool", [this](DPrinter & pr, Stmt * s)
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


	// <utils>
	finder.addMatcher(callExpr(callee(functionDecl(matchesName("^::std::swap$"), parameterCountIs(2)))).bind("std::swap"), this);
	stmtPrinters.emplace("std::swap", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "std.algorithm.swap(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ", ";
			pr.TraverseStmt(memCall->getArg(1));
			pr.stream() << ")";
			pr.addExternInclude("std.algorithm");
		}
	});

	// <ctime>
	finder.addMatcher(callExpr(callee(functionDecl(matchesName("^(::std)?::time$")))).bind("std::time"), this);
	stmtPrinters.emplace("std::time", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "core.stdc.time.time(";
			pr.TraverseStmt(memCall->getArg(0));
			pr.stream() << ")";
			pr.addExternInclude("core.stdc.time");
		}
	});

	//BOOST_THROW_EXCEPTION
	finder.addMatcher(callExpr(callee(functionDecl(matchesName("^::boost::exception_detail::throw_exception_$")))).bind("throw_exception_"), this);
	stmtPrinters.emplace("throw_exception_", [this](DPrinter & pr, Stmt * s)
	{
		if(auto* memCall = dyn_cast<CallExpr>(s))
		{
			pr.stream() << "throw ";
			pr.TraverseStmt(*memCall->arg_begin());
		}
	});

	//boost::serialisation
	finder.addMatcher(recordType(hasDeclaration(namedDecl(hasName("boost::serialization::access")))).bind("boost::serialization::access"), this);
	typePrinters.emplace("boost::serialization::access", [this](DPrinter & pr, Type*) {pr.stream() << "int"; });


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
	for(auto const& name_n_func : on_decl_match)
	{
		if(auto* decl = Result.Nodes.getNodeAs<Decl>(name_n_func.first))
			name_n_func.second(decl);
	}
	for(auto const& name_n_func : on_stmt_match)
	{
		if(auto* stmt = Result.Nodes.getNodeAs<Stmt>(name_n_func.first))
			name_n_func.second(stmt);
	}
	for(auto const& name_n_func : on_type_match)
	{
		if(auto* type = Result.Nodes.getNodeAs<Type>(name_n_func.first))
			name_n_func.second(type);
	}
}


std::function<void(DPrinter& printer, clang::Stmt*)> MatchContainer::getPrinter(clang::Stmt const* node) const
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

std::function<void(DPrinter& printer, clang::Decl*)> MatchContainer::getPrinter(clang::Decl const* node) const
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

std::function<void(DPrinter& printer, clang::Type*)> MatchContainer::getPrinter(clang::Type const* node) const
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
