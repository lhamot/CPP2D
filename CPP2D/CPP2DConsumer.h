#pragma once

#pragma warning(push, 0)
#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#pragma warning(pop)

#include "MatchContainer.h"
#include "DPrinter.h"

namespace clang
{
class CompilerInstance;
}

class VisitorToDConsumer : public clang::ASTConsumer
{
public:
	explicit VisitorToDConsumer(
	  clang::CompilerInstance& Compiler,
	  llvm::StringRef InFile
	);

	virtual void HandleTranslationUnit(clang::ASTContext& Context);

private:
	clang::CompilerInstance& Compiler;
	MatchContainer receiver;
	clang::ast_matchers::MatchFinder finder;
	std::unique_ptr<clang::ASTConsumer> finderConsumer;
	std::string InFile;
	DPrinter Visitor;
};

