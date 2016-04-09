#include "CPP2DConsumer.h"

#pragma warning(push, 0)
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/Support/Path.h>
#pragma warning(pop)

#include "CPP2DPPHandling.h"

#include <fstream>
#include <sstream>

VisitorToDConsumer::VisitorToDConsumer(
  clang::CompilerInstance& Compiler,
  llvm::StringRef InFile
)
	: Compiler(Compiler)
	, finder(receiver.getMatcher())
	, finderConsumer(finder.newASTConsumer())
	, InFile(InFile.str())
	, Visitor(&Compiler.getASTContext(), receiver, InFile)
{
}

void VisitorToDConsumer::HandleTranslationUnit(clang::ASTContext& Context)
{
	//Find_Includes
	auto& ppcallback = dynamic_cast<CPP2DPPHandling&>(*Compiler.getPreprocessor().getPPCallbacks());
	auto& incs = ppcallback.getIncludes();

	finderConsumer->HandleTranslationUnit(Context);
	Visitor.setIncludes(incs);
	Visitor.TraverseTranslationUnitDecl(Context.getTranslationUnitDecl());

	std::string modulename = llvm::sys::path::stem(InFile).str();
	std::ofstream file(modulename + ".d");
	std::string new_modulename;
	std::replace_copy(std::begin(modulename), std::end(modulename), std::back_inserter(new_modulename), '-', '_'); //Replace illegal characters
	if(new_modulename != modulename)  // When filename has some illegal characters
		file << "module " << new_modulename << ';';
	for(auto const& import : Visitor.getExternIncludes())
		file << "import " << import << ";" << std::endl;
	file << "\n\n";
	for(auto const& code : ppcallback.getInsertedBeforeDecls())
		file << code << '\n';
	file << Visitor.getDCode();
}