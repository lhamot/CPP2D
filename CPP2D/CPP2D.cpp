//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma warning(push, 0)
#pragma warning(disable: 4548)
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#pragma warning(pop)

#include <iostream>
#include <fstream>

#include "CPP2DFrontendAction.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

cl::list<std::string> MacroAsExpr(
  "macro-expr",
  cl::desc("This macro will not be expanded and will be migrated expecting an expression"),
  cl::cat(MyToolCategory),
  cl::ZeroOrMore);

cl::list<std::string> MacroAsStmt(
  "macro-stmt",
  cl::desc("This macro will not be expanded and will be migrated expecting statments"),
  cl::cat(MyToolCategory),
  cl::ZeroOrMore);


//! Used to add fake options to the compiler
//!  - Like for example : -fno-delayed-template-parsing
class CPP2DCompilationDatabase : public clang::tooling::CompilationDatabase
{
	clang::tooling::CompilationDatabase& sourceCDB;

public:
	CPP2DCompilationDatabase(clang::tooling::CompilationDatabase& sourceCDB_)
		: sourceCDB(sourceCDB_)
	{
	}

	std::vector<CompileCommand> getCompileCommands(StringRef FilePath) const override
	{
		std::vector<CompileCommand> result = sourceCDB.getCompileCommands(FilePath);
		for(CompileCommand& cc : result)
		{
			cc.CommandLine.push_back("-fno-delayed-template-parsing");
			cc.CommandLine.push_back("-ferror-limit=999999");
			cc.CommandLine.push_back("-Wno-builtin-macro-redefined");
			cc.CommandLine.push_back("-Wno-unused-value");
			cc.CommandLine.push_back("-DCPP2D");
		}
		return result;
	}

	std::vector<std::string> getAllFiles() const override { return sourceCDB.getAllFiles(); }

	std::vector<CompileCommand> getAllCompileCommands() const override
	{
		assert(false && "Unexpected call to function CPP2DCompilationDatabase::getAllCompileCommands");
		return sourceCDB.getAllCompileCommands();
	}
};

int main(int argc, char const** argv)
{
	std::vector<char const*> argv_vect;
	std::copy(argv, argv + static_cast<intptr_t>(argc), std::back_inserter(argv_vect));
	argv_vect.insert(std::begin(argv_vect) + 1, "-macro-expr=assert/e");
	argc = static_cast<int>(argv_vect.size());
	CommonOptionsParser OptionsParser(argc, argv_vect.data(), MyToolCategory);
	CPP2DCompilationDatabase compilationDatabase(OptionsParser.getCompilations());
	ClangTool Tool(
	  compilationDatabase,
	  OptionsParser.getSourcePathList());
	return Tool.run(newFrontendActionFactory<CPP2DFrontendAction>().get());
}