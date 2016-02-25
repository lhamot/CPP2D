#pragma warning(push, 0)
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#pragma warning(pop)

#include <iostream>
#include <fstream>

#include "VisitorToD.h"

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


int main(int argc, const char **argv) 
{
	CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
	ClangTool Tool(
		OptionsParser.getCompilations(),
		OptionsParser.getSourcePathList());
	return Tool.run(newFrontendActionFactory<VisitorToDAction>().get());
}