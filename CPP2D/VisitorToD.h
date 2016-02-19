#pragma once

#include <memory>

#pragma warning(push, 0)
#include "clang/Frontend/FrontendActions.h"
#pragma warning(pop)

class VisitorToD;

namespace clang
{
	class CompilerInstance;
}


class VisitorToDAction : public clang::ASTFrontendAction
{
public:
	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
		clang::CompilerInstance &Compiler,
		llvm::StringRef //InFile
		);

	bool BeginSourceFileAction(clang::CompilerInstance &ci, llvm::StringRef);

	void EndSourceFileAction();
};
