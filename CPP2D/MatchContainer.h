#pragma once

#pragma warning(push, 0)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#pragma warning(pop)

#include <unordered_map>
#include <unordered_set>

class MatchContainer : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
	void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;

	std::unordered_set<clang::VarDecl const*> forrange_loopvar;
	std::unordered_set<clang::AutoType const*> forrange_loopvar_auto;
	std::unordered_set<clang::LValueReferenceType const*> ref_to_class;
	std::unordered_map<std::string, clang::CXXMethodDecl const*> hash_traits;
	std::unordered_set<clang::Decl const*> dont_print_this_decl;
	std::unordered_multimap<std::string, clang::FunctionDecl const*> free_operator;  //left operand will become this
	std::unordered_multimap<std::string, clang::FunctionDecl const*> free_operator_right; //right operand will become this
};