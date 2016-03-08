#pragma once

#include <memory>

#pragma warning(push, 0)
#include "clang/Frontend/FrontendActions.h"
#pragma warning(pop)

#include "MatchContainer.h"

namespace clang
{
class CompilerInstance;

namespace ast_matchers
{
class MatchFinder;
}
}

class VisitorToD;
class MatchContainer;

class VisitorToDAction : public clang::ASTFrontendAction
{
public:
	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
	  clang::CompilerInstance& Compiler,
	  llvm::StringRef //InFile
	) override;

	bool BeginSourceFileAction(clang::CompilerInstance& ci, llvm::StringRef) override;

	void EndSourceFileAction() override;
};
