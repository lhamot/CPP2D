#include "CPP2DConsumer.h"

#pragma warning(push, 0)
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/Support/Path.h>
#pragma warning(pop)

#include "Find_Includes.h"

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
	auto& ppcallback = dynamic_cast<Find_Includes&>(*Compiler.getPreprocessor().getPPCallbacks());
	auto& incs = ppcallback.includes_in_file;
	includes_in_file.insert(incs.begin(), incs.end());

	outStack.clear();
	outStack.emplace_back(std::make_unique<std::stringstream>());

	finderConsumer->HandleTranslationUnit(Context);
	Visitor.TraverseTranslationUnitDecl(Context.getTranslationUnitDecl());

	std::string modulename = llvm::sys::path::stem(InFile).str();
	std::ofstream file(modulename + ".d");
	std::string new_modulename;
	std::replace_copy(std::begin(modulename), std::end(modulename), std::back_inserter(new_modulename), '-', '_'); //Replace illegal characters
	if(new_modulename != modulename)  // When filename has some illegal characters
		file << "module " << new_modulename << ';';
	for(auto const& import : Visitor.extern_includes)
		file << "import " << import << ";" << std::endl;
	file << "\n\n";
	for(auto const& code : ppcallback.add_before_decl)
		file << code << '\n';
	file << out().str();
}