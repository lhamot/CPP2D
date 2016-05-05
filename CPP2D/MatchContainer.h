//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <unordered_map>
#include <unordered_set>

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/ASTMatchers/ASTMatchFinder.h>
#pragma warning(pop)

class DPrinter;

namespace clang
{
	class Stmt;
	class Decl;
	class Type;
}

class MatchContainer : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
	clang::ast_matchers::MatchFinder getMatcher();

	void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;

	std::unordered_map<std::string, clang::CXXMethodDecl const*> hashTraits;
	std::unordered_multimap<std::string, clang::FunctionDecl const*> freeOperator;  // left operand will become this
	std::unordered_multimap<std::string, clang::FunctionDecl const*> freeOperatorRight; // right operand will become this

	std::function<void(DPrinter& printer, clang::Stmt*)> getPrinter(clang::Stmt const*) const;
	std::function<void(DPrinter& printer, clang::Decl*)> getPrinter(clang::Decl const*) const;
	std::function<void(DPrinter& printer, clang::Type*)> getPrinter(clang::Type const*) const;

	std::unordered_multimap<clang::Stmt const*, std::string> stmtTags;
	std::unordered_multimap<clang::Decl const*, std::string> declTags;
	std::unordered_multimap<clang::Type const*, std::string> typeTags;

private:
	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Type*)>> typePrinters;
	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Stmt*)>> stmtPrinters;
	std::unordered_map<std::string, std::function<void(DPrinter& printer, clang::Decl*)>> declPrinters;

	std::unordered_map<std::string, std::function<void(clang::Stmt const*)>> onStmtMatch;
	std::unordered_map<std::string, std::function<void(clang::Decl const*)>> onDeclMatch;
	std::unordered_map<std::string, std::function<void(clang::Type const*)>> onTypeMatch;
};