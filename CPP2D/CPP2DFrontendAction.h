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

class CPP2DPPHandling;

//! Implement clang::ASTFrontendAction to create the CPP2DConsumer
class CPP2DFrontendAction : public clang::ASTFrontendAction
{
public:
	//! Create the CPP2DConsumer
	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
	  clang::CompilerInstance& Compiler,
	  llvm::StringRef InFile
	) override;

	//! Add the CPP2DPPHandling (PPCallbacks) to the Preprocessor
	bool BeginSourceFileAction(clang::CompilerInstance& ci) override;

private:
	CPP2DPPHandling* ppHandlingPtr = nullptr;
};
