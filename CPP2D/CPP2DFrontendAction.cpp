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
	auto visitor = std::make_unique<VisitorToDConsumer>(Compiler, InFile);
	return std::move(visitor);
}

bool CPP2DFrontendAction::BeginSourceFileAction(CompilerInstance& ci, StringRef file)
{
	Preprocessor& pp = ci.getPreprocessor();
	pp.addPPCallbacks(std::make_unique<CPP2DPPHandling>(pp.getSourceManager(), pp, file));
	return true;
}

void CPP2DFrontendAction::EndSourceFileAction()
{
}
