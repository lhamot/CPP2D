#pragma warning(push, 0)
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

cl::list<std::string> MacroAsExpr("macro-expr",
                                  cl::desc("This macro will not be expanded and will be migrated expecting an expression"),
                                  cl::cat(MyToolCategory),
                                  cl::ZeroOrMore);

cl::list<std::string> MacroAsStmt("macro-stmt",
                                  cl::desc("This macro will not be expanded and will be migrated expecting statments"),
                                  cl::cat(MyToolCategory),
                                  cl::ZeroOrMore);

int main(int argc, char const** argv)
{
	std::vector<char const*> argv_vect;
	std::copy(argv, argv + argc, std::back_inserter(argv_vect));
	argv_vect.push_back("-fno-delayed-template-parsing");
	argv_vect.push_back("-ferror-limit=999999");
	argv_vect.push_back("-Wno-builtin-macro-redefined");
	argv_vect.push_back("-Wno-unused-value");
	argv_vect.push_back("-DCPP2D");
	argv_vect.push_back("-D__LINE__=(\"__CPP2D__LINE__\", 0)");
	argv_vect.push_back("-D__FILE__=(\"__CPP2D__FILE__\", \"\")");
	argv_vect.push_back("-D__FUNCTION__=(\"__CPP2D__FUNC__\", __FUNCTION__)");
	argv_vect.push_back("-D__PRETTY_FUNCTION__=(\"__CPP2D__PFUNC__\", __PRETTY_FUNCTION__)");
	argv_vect.push_back("-D__func__=(\"__CPP2D__func__\", __func__)");
	argc = static_cast<int>(argv_vect.size());
	CommonOptionsParser OptionsParser(argc, argv_vect.data(), MyToolCategory);
	ClangTool Tool(
	  OptionsParser.getCompilations(),
	  OptionsParser.getSourcePathList());
	return Tool.run(newFrontendActionFactory<CPP2DFrontendAction>().get());
}