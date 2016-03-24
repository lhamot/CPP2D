#pragma warning(push, 0)
#pragma warning(disable, 4702)
#include <clang/Lex/PPCallbacks.h>
#include <llvm/ADT/StringRef.h>
#pragma warning(pop)

namespace clang
{
//class Preprocessor;
//class PPCallbacks;
//class SourceLocation;
//class CharSourceRange;
}
namespace llvm
{

}

class Find_Includes : public clang::PPCallbacks
{
public:
	Find_Includes(clang::Preprocessor& pp, llvm::StringRef inFile);

	void InclusionDirective(
	  clang::SourceLocation,		//hash_loc,
	  const clang::Token&,		//include_token,
	  llvm::StringRef file_name,
	  bool,						//is_angled,
	  clang::CharSourceRange,		//filename_range,
	  const clang::FileEntry*,	//file,
	  llvm::StringRef,			//search_path,
	  llvm::StringRef,			//relative_path,
	  const clang::Module*		//imported
	) override;

	void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override;

	void MacroExpands(const clang::Token& MacroNameTok,
	                  const clang::MacroDefinition& MD, clang::SourceRange Range,
	                  const clang::MacroArgs* Args) override;

	std::set<std::string> includes_in_file;
	std::set<std::string> add_before_decl;
private:
	clang::Preprocessor& pp_;
	llvm::StringRef inFile_;
	llvm::StringRef modulename_;
	std::set<std::string> macro_to_transform;
};