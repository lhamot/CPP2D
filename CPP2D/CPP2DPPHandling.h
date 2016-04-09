#pragma warning(push, 0)
#pragma warning(disable, 4702)
#include <clang/Lex/PPCallbacks.h>
#include <llvm/ADT/StringRef.h>
#pragma warning(pop)

class CPP2DPPHandling : public clang::PPCallbacks
{
public:
	CPP2DPPHandling(clang::Preprocessor& pp, llvm::StringRef inFile);

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
	void CPP2DPPHandling::inject_macro(
	  clang::MacroDirective const* MD,
	  std::string const& name,
	  std::string const& new_macro);
	void TransformMacroExpr(clang::Token const& MacroNameTok,
	                        clang::MacroDirective const* MD,
	                        std::string const& name,
	                        std::string const& args);
	void TransformMacroStmt(clang::Token const& MacroNameTok,
	                        clang::MacroDirective const* MD,
	                        std::string const& name,
	                        std::string const& args);

	clang::Preprocessor& pp_;
	llvm::StringRef inFile_;
	llvm::StringRef modulename_;
	std::map<std::string, std::string> macro_expr;
	std::map<std::string, std::string> macro_stmt;

	std::set<std::string> includes_in_file;
	std::set<std::string> add_before_decl;
};