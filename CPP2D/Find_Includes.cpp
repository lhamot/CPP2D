#include "Find_Includes.h"

#pragma warning(push, 0)
#pragma warning(disable, 4702)
#include <llvm/Support/Path.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#pragma warning(pop)

using namespace clang;
using namespace llvm;

std::string predefines;
std::set<std::string> new_macros;
std::set<std::string> new_macros_name;
extern cl::list<std::string> MacroAsFunction;

Find_Includes::Find_Includes(Preprocessor& pp, StringRef inFile)
	: pp_(pp)
	, inFile_(inFile)
	, modulename_(llvm::sys::path::stem(inFile))
	, macro_to_transform(std::begin(MacroAsFunction), std::end(MacroAsFunction))
{
	//! @todo : Find a better way if it exists
	predefines = pp_.getPredefines();
	predefines += "\nvoid cpp2d_dummy_variadic(...);\n";
	pp_.setPredefines(predefines);
}

void Find_Includes::InclusionDirective(
  SourceLocation,		//hash_loc,
  const Token&,			//include_token,
  StringRef file_name,
  bool,					//is_angled,
  CharSourceRange,		//filename_range,
  const FileEntry*,		//file,
  StringRef,			//search_path,
  StringRef,			//relative_path,
  const Module*			//imported
)
{
	includes_in_file.insert(file_name);
}

void Find_Includes::MacroDefined(const Token& MacroNameTok, const MacroDirective* MD)
{
	MacroInfo const* MI = MD->getMacroInfo();

	std::string const name = MacroNameTok.getIdentifierInfo()->getName();
	if(new_macros_name.count(name) || MI->isBuiltinMacro() || !macro_to_transform.count(name))
		return;
	pp_.appendMacroDirective(MacroNameTok.getIdentifierInfo(), new UndefMacroDirective(MacroNameTok.getLocation()));

	new_macros_name.insert(name);
	std::string new_macro = "\n#define " + name;
	auto print_macro_args = [MI](std::string & new_macro, char const* prefix = nullptr)
	{
		new_macro += '(';
		bool first = true;
		for(IdentifierInfo const* arg : MI->args())
		{
			if(first)
				first = false;
			else
				new_macro += ", ";
			if(prefix)
				new_macro += prefix;
			new_macro += arg->getName();
		}
		new_macro += ')';
	};

	if(MI->isFunctionLike())
		print_macro_args(new_macro);
	new_macro += " (\"CPP2D_MACRO_EXPAND\", ((\"" + name + "\", cpp2d_dummy_variadic";
	if(MI->isFunctionLike())
		print_macro_args(new_macro);
	new_macro += "), ";

	bool first = true;
	for(Token const& Tok : MI->tokens())
	{
		if(first || Tok.hasLeadingSpace())
			new_macro += " ";
		first = false;

		if(const char* Punc = tok::getPunctuatorSpelling(Tok.getKind()))
			new_macro += Punc;
		else if(Tok.isLiteral() && Tok.getLiteralData())
			new_macro.append(Tok.getLiteralData(), Tok.getLength());
		else if(auto* II = Tok.getIdentifierInfo())
			new_macro += II->getName();
		else
			new_macro += Tok.getName();
	}
	new_macro += "))\n";

	errs() << new_macro;

	//! @todo : Find a better way if it exists
	auto iter_inserted = new_macros.insert(new_macro);
	std::unique_ptr<MemoryBuffer> membuf =
	  MemoryBuffer::getMemBuffer(*iter_inserted.first, "<" + name + "_macro_override>");
	FileID fileID = pp_.getSourceManager().createFileID(std::move(membuf));
	pp_.EnterSourceFile(fileID, pp_.GetCurDirLookup(), MD->getMacroInfo()->getDefinitionEndLoc());

	// Make D template

	std::set<std::string> arg_names;
	for(IdentifierInfo const* arg : MI->args())
		arg_names.insert(arg->getName());

	std::string d_templ = "template " + name;
	if(MI->isFunctionLike())
		print_macro_args(d_templ, "string ");
	d_templ += "\n{\n";
	d_templ += "    const char[] " + name + " = ";
	first = true;
	bool next_is_str = false;
	bool next_is_pasted = false;
	for(Token const& Tok : MI->tokens())
	{
		if(!first && Tok.hasLeadingSpace())
			d_templ += "\" \"";
		first = false;

		if(const char* Punc = tok::getPunctuatorSpelling(Tok.getKind()))
		{
			if(Punc == std::string("#"))
				next_is_str = true;
			else if(Punc == std::string("##"))
				next_is_pasted = true;
			else
				d_templ += std::string(1, '"') + Punc + '"';
		}
		else if(Tok.isLiteral() && Tok.getLiteralData())
			d_templ += '"' + std::string(Tok.getLiteralData(), Tok.getLength()) + '"';
		else if(auto* II = Tok.getIdentifierInfo())
		{
			if(arg_names.count(II->getName()))
			{
				if(next_is_str)
				{
					d_templ += "\"q{\" ~ " + std::string(II->getName()) + " ~ \"}\"";
					next_is_str = false;
				}
				else if(next_is_pasted)
				{
					d_templ += " ~ " + II->getName().str() + " ~ ";
					next_is_pasted = false;
				}
				else
					d_templ += "\" \" ~ " + II->getName().str() + " ~ \" \"";
			}
			else
				d_templ += '"' + II->getName().str() + '"';
		}
		else
			d_templ += std::string(1, '"') + Tok.getName() + '"';
	}
	d_templ += ";\n";
	d_templ += "}\n";

	size_t pos = std::string::npos;
	while((pos = d_templ.find("\"\"")) != std::string::npos)
		d_templ = d_templ.substr(0, pos) + d_templ.substr(pos + 2);

	add_before_decl.insert(d_templ);
}

void Find_Includes::MacroExpands(
  const clang::Token&, //MacroNameTok,
  const clang::MacroDefinition&, //MD,
  clang::SourceRange, //Range,
  const clang::MacroArgs* //Args
)
{
}