#pragma once

#pragma warning(push, 0)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#pragma warning(pop)


class MatchContainer : public clang::ast_matchers::MatchFinder::MatchCallback 
{
public:
	void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;

	std::set<clang::VarDecl const*> forrange_loopvar;
	std::set<clang::AutoType const*> forrange_loopvar_auto;
	std::set<clang::LValueReferenceType const*> ref_to_class;
};