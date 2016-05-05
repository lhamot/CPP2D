//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
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
