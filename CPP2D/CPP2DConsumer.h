//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#pragma warning(push, 0)
#include <clang/AST/ASTConsumer.h>
#pragma warning(disable: 4265)
#include <clang/ASTMatchers/ASTMatchers.h>
#pragma warning(pop)

#include "MatchContainer.h"
#include "DPrinter.h"

namespace clang
{
class CompilerInstance;
}

class CPP2DPPHandling;

//! Implement clang::ASTConsumer to call the DPrinter
class CPP2DConsumer : public clang::ASTConsumer
{
public:
	explicit CPP2DConsumer(
	  clang::CompilerInstance& compiler,
	  llvm::StringRef inFile
	);

	//! Print imports, mixins, and finaly call the DPrinter on the translationUnit
	void HandleTranslationUnit(clang::ASTContext& context) override;

	void setPPCallBack(CPP2DPPHandling* cb)
	{
		ppcallbackPtr = cb;
	}

private:
	clang::CompilerInstance& compiler;
	MatchContainer receiver;
	clang::ast_matchers::MatchFinder finder;
	std::unique_ptr<clang::ASTConsumer> finderConsumer;
	std::string inFile;
	DPrinter visitor;
	CPP2DPPHandling* ppcallbackPtr = nullptr;
};

