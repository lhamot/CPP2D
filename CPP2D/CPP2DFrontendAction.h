#pragma once

#include <memory>

#pragma warning(push, 0)
#include "clang/Frontend/FrontendActions.h"
#pragma warning(pop)

namespace clang
{
class CompilerInstance;
}

class CPP2DFrontendAction : public clang::ASTFrontendAction
{
public:
	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
	  clang::CompilerInstance& Compiler,
	  llvm::StringRef InFile
	) override;

	bool BeginSourceFileAction(clang::CompilerInstance& ci, llvm::StringRef) override;

	void EndSourceFileAction() override;

private:
	llvm::StringRef modulename;
};
