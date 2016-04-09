#pragma once

#pragma warning(push, 0)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#pragma warning(pop)

#include <unordered_map>
#include <unordered_set>

class DPrinter;

class MatchContainer : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
	clang::ast_matchers::MatchFinder getMatcher();

	void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;

	std::unordered_map<std::string, clang::CXXMethodDecl const*> hash_traits;
	std::unordered_multimap<std::string, clang::FunctionDecl const*> free_operator;  //left operand will become this
	std::unordered_multimap<std::string, clang::FunctionDecl const*> free_operator_right; //right operand will become this

	std::function<void(DPrinter& printer, clang::Stmt const*)> getPrinter(clang::Stmt const*) const;
	std::function<void(DPrinter& printer, clang::Decl const*)> getPrinter(clang::Decl const*) const;
	std::function<void(DPrinter& printer, clang::Type const*)> getPrinter(clang::Type const*) const;

private:
	std::unordered_multimap<clang::Stmt const*, std::string> stmtTags;
	std::unordered_multimap<clang::Decl const*, std::string> declTags;
	std::unordered_multimap<clang::Type const*, std::string> typeTags;

	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Type const*)>> typePrinters;
	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Stmt const*)>> stmtPrinters;
	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Decl const*)>> declPrinters;

	std::unordered_map<std::string, std::function<void(clang::Stmt const*)>> on_stmt_match;
	std::unordered_map<std::string, std::function<void(clang::Decl const*)>> on_decl_match;
	std::unordered_map<std::string, std::function<void(clang::Type const*)>> on_type_match;
};