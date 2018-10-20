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

//! Store matchers and receive the callbacks when the are find
//!
//! Find some paterns in the source which are hard to finc while parsing
//! Permit the excecution of custom matcher and custom printer in order to
//!     translate external library usages
class MatchContainer : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
	//! Generate all ASTMatchers
	clang::ast_matchers::MatchFinder getMatcher();

	//! Hash traits (std::hash) of this record. [recordname] -> method
	std::unordered_map<std::string, clang::CXXMethodDecl const*> hashTraits;
	//! @brief Free operators (left) of this record. [recordname] -> operator
	//! @remark The record appear on the left side
	std::unordered_multimap<std::string, clang::FunctionDecl const*> freeOperator;
	//! @brief Free operators (right) of this record. [recordname] -> operator
	//! @remark The record appear on the right side
	std::unordered_multimap<std::string, clang::FunctionDecl const*> freeOperatorRight;

	typedef std::function<void(DPrinter& printer, clang::Stmt*)> StmtPrinter; //!< Custom Stmt printer
	typedef std::function<void(DPrinter& printer, clang::Decl*)> DeclPrinter; //!< Custom Decl printer
	typedef std::function<void(DPrinter& printer, clang::Type*)> TypePrinter; //!< Custom Type printer


	StmtPrinter getPrinter(clang::Stmt const*) const; //!< Get the custom printer for this statment
	DeclPrinter getPrinter(clang::Decl const*) const; //!< Get the custom printer for this decl
	TypePrinter getPrinter(clang::Type const*) const; //!< Get the custom printer for this type

	//! Get the nth template argument of type tmplType
	static clang::TemplateArgument const* getTemplateTypeArgument(
	  clang::Expr const* tmplType,
	  size_t nth);

	//! Change the name of a C++ type
	void rewriteType(
	  clang::ast_matchers::MatchFinder& finder,
	  std::string const& oldName,	//!< Old type name in C++ (no regex)
	  std::string const& newName,	//!< New type name in D
	  std::string const& newImport	//!< import to get this type in **D**
	);

	//! Custom print a method call
	void methodPrinter(
	  std::string const& className,		//!< Class of this method (no regex)
	  std::string const& methodName,	//!< Metod name (no regex)
	  StmtPrinter const& printer		//!< Custom printer
	);

	//! Custom print a global function call
	void globalFuncPrinter(
	  std::string const& funcName,	//!< Function name (regex)
	  StmtPrinter const& printer	//!< Custom printer
	);

	//! Custom print a template type
	void tmplTypePrinter(
	  std::string const& name,			//!< Type name (regex)
	  DeclPrinter const& printMapDecl	//!< Custom printer
	);

	//! Custom print a member call
	void memberPrinter(
	  clang::ast_matchers::MatchFinder& finder,
	  std::string const& regexpr,	//!< Fully qualified member name (regex)
	  StmtPrinter const& printer	//!< Custom printer
	);

	//! Custom print an operator call
	void operatorCallPrinter(
	  clang::ast_matchers::MatchFinder& finder,
	  std::string const& classRegexpr,	//!< class name (regex)
	  std::string const& op,			//!< operator (like "==")
	  StmtPrinter const& printer		//!< Custom printer
	);

	//! Port a C standard library function to D
	void cFuncPrinter(
	  std::string const& lib,	//!< include name
	  std::string const& func	//!< function name (no regex)
	);

	//! clang::Stmt which match. [clang::Stmt] -> matchername
	std::unordered_multimap<clang::Stmt const*, std::string> stmtTags;
	//! clang::Stmt which match. [clang::Decl] -> matchername
	std::unordered_multimap<clang::Decl const*, std::string> declTags;
	//! clang::Stmt which match. [clang::Type] -> matchername
	std::unordered_multimap<clang::Type const*, std::string> typeTags;

	typedef std::unordered_map<std::string, StmtPrinter> ClassPrinter;
	//! How to print a call to this method. methodPrinters[method_name][class_name] => printer
	std::unordered_map<std::string, ClassPrinter> methodPrinters;

	std::unordered_map<std::string, StmtPrinter> globalFuncPrinters;

	std::unordered_map<std::string, DeclPrinter> customTypePrinters;

	//! Custom printer for clang::Type matchers. [matchername] -> printer
	std::unordered_map<std::string, TypePrinter> typePrinters;
	//! Custom printer for clang::Stmt matchers. [matchername] -> printer
	std::unordered_map<std::string, StmtPrinter> stmtPrinters;
	//! Custom printer for clang::Decl matchers. [matchername] -> printer
	std::unordered_map<std::string, DeclPrinter> declPrinters;

	//! Function excecuted when a clang::Stmt match. [matchername] -> function
	std::unordered_map<std::string, std::function<void(clang::Stmt const*)>> onStmtMatch;
	//! Function excecuted when a clang::Decl match. [matchername] -> function
	std::unordered_map<std::string, std::function<void(clang::Decl const*)>> onDeclMatch;
	//! Function excecuted when a clang::Type match. [matchername] -> function
	std::unordered_map<std::string, std::function<void(clang::Type const*)>> onTypeMatch;

private:
	//! When match is find, excecute on*Match or add the node to *Tags
	void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;
};