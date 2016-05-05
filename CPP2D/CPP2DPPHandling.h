//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#pragma warning(push, 0)
#include <clang/Lex/PPCallbacks.h>
#include <llvm/ADT/StringRef.h>
#pragma warning(pop)

namespace clang
{
class ASTContext;
}

//! Extract includes and transform macros
class CPP2DPPHandling : public clang::PPCallbacks
{
public:
	CPP2DPPHandling(clang::SourceManager& sourceManager,
	                clang::Preprocessor& pp,
	                llvm::StringRef inFile);

	void InclusionDirective(
	  clang::SourceLocation,		//hash_loc,
	  const clang::Token&,			//include_token,
	  llvm::StringRef file_name,
	  bool,							//is_angled,
	  clang::CharSourceRange,		//filename_range,
	  const clang::FileEntry*,		//file,
	  llvm::StringRef,				//search_path,
	  llvm::StringRef,				//relative_path,
	  const clang::Module*			//imported
	) override;

	void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override;

	void MacroExpands(const clang::Token& MacroNameTok,
	                  const clang::MacroDefinition& MD, clang::SourceRange Range,
	                  const clang::MacroArgs* Args) override;

	std::set<std::string> const& getIncludes() const;
	std::set<std::string> const& getInsertedBeforeDecls() const;

private:
	void inject_macro(
	  clang::MacroDirective const* MD,
	  std::string const& name,
	  std::string const& new_macro);
	struct MacroInfo
	{
		std::string name;
		std::string argType;
		std::string cppReplace;
	};
	void TransformMacroExpr(clang::Token const& MacroNameTok,
	                        clang::MacroDirective const* MD,
	                        CPP2DPPHandling::MacroInfo const& args);
	void TransformMacroStmt(clang::Token const& MacroNameTok,
	                        clang::MacroDirective const* MD,
	                        CPP2DPPHandling::MacroInfo const& args);

	clang::SourceManager& sourceManager;
	clang::Preprocessor& pp;
	llvm::StringRef inFile;
	llvm::StringRef modulename;
	std::map<std::string, MacroInfo> macro_expr;
	std::map<std::string, MacroInfo> macro_stmt;

	std::set<std::string> includes_in_file;
	std::set<std::string> add_before_decl;
};