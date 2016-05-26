//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "CPP2DFrontendAction.h"

#pragma warning(push, 0)
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#pragma warning(pop)

#include "CPP2DConsumer.h"
#include "CPP2DPPHandling.h"

using namespace clang;

std::unique_ptr<clang::ASTConsumer> CPP2DFrontendAction::CreateASTConsumer(
  clang::CompilerInstance& Compiler,
  llvm::StringRef InFile
)
{
	return std::make_unique<CPP2DConsumer>(Compiler, InFile);
}

bool CPP2DFrontendAction::BeginSourceFileAction(CompilerInstance& ci, StringRef file)
{
	Preprocessor& pp = ci.getPreprocessor();
	pp.addPPCallbacks(std::make_unique<CPP2DPPHandling>(pp.getSourceManager(), pp, file));
	return true;
}

