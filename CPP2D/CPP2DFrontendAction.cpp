//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "CPP2DFrontendAction.h"

#include <memory>

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
	auto consumer = std::make_unique<CPP2DConsumer>(Compiler, InFile);
	consumer->setPPCallBack(ppHandlingPtr);
	return std::move(consumer);
}

bool CPP2DFrontendAction::BeginSourceFileAction(CompilerInstance& ci)
{
	Preprocessor& pp = ci.getPreprocessor();
	auto ppHandling = std::make_unique<CPP2DPPHandling>(pp.getSourceManager(), pp, getCurrentFile());
	ppHandlingPtr = ppHandling.get();
	pp.addPPCallbacks(std::move(ppHandling));
	return true;
}

