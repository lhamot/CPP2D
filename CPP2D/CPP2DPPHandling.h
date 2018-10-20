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

#include <set>

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

	//! Fill the list of included files (includes_in_file)
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

	//! Transform macro definition by calling TransformMacroExpr or TransformMacroStmt
	void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override;

	//! Get include list
	std::set<std::string> const& getIncludes() const;
	//! Get macros to add in the D code
	std::set<std::string> const& getInsertedBeforeDecls() const;

private:
	//! Add or replace a macro definition in the clang::Preprocessor
	void inject_macro(
	  clang::MacroDirective const* MD,
	  std::string const& name,
	  std::string const& new_macro);

	//! Options passed by the user about a specific macro
	struct MacroInfo
	{
		std::string name;		//!< Macro name
		std::string argType;	//!< e:expression n:name t:type
		std::string cppReplace;	//!< override the macro, in C++ code
	};
	//! @brief Transform macro definition if tagged by -macro-expr option
	//! + Add the D mixin version in the list add_before_decl
	void TransformMacroExpr(clang::Token const& MacroNameTok,
	                        clang::MacroDirective const* MD,
	                        CPP2DPPHandling::MacroInfo const& args);
	//! @brief Transform macro definition if tagged by -macro-stmt option
	//! + Add the D mixin version in the list add_before_decl
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

	std::string predefines;
	std::set<std::string> new_macros;
};