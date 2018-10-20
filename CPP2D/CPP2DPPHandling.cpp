//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "CPP2DPPHandling.h"

#pragma warning(push, 0)
#include <llvm/Support/Path.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/AST/ASTContext.h>
#pragma warning(pop)

#include <sstream>
#include "CPP2DTools.h"

using namespace clang;
using namespace llvm;

extern cl::list<std::string> MacroAsExpr;
extern cl::list<std::string> MacroAsStmt;

CPP2DPPHandling::CPP2DPPHandling(clang::SourceManager& sourceManager_,
                                 Preprocessor& pp_,
                                 StringRef inFile_)
	: sourceManager(sourceManager_)
	, pp(pp_)
	, inFile(inFile_)
	, modulename(llvm::sys::path::stem(inFile_))
{
	auto split = [](std::string name)
	{
		std::string args, cppReplace;
		size_t arg_pos = name.find("/");
		if(arg_pos != std::string::npos)
		{
			args = name.substr(arg_pos + 1);
			name = name.substr(0, arg_pos);
		}
		arg_pos = args.find("/");
		if(arg_pos != std::string::npos)
		{
			cppReplace = args.substr(arg_pos + 1);
			args = args.substr(0, arg_pos);
		}
		return std::make_tuple(name, args, cppReplace);
	};
	for(std::string const& macro_options : MacroAsExpr)
	{
		std::string name, args, cppReplace;
		std::tie(name, args, cppReplace) = split(macro_options);
		macro_expr.emplace(name, MacroInfo{ name, args, cppReplace });
	}
	for(std::string const& macro_options : MacroAsStmt)
	{
		std::string name, args, cppReplace;
		std::tie(name, args, cppReplace) = split(macro_options);
		macro_stmt.emplace(name, MacroInfo{ name, args, cppReplace });
	}

	// TODO : Find a better way if it exists
	predefines = pp_.getPredefines() +
	             "\ntemplate<typename... Args> int cpp2d_dummy_variadic(Args&&...){}\n"
	             "template<typename T> int cpp2d_type();\n"
	             "int cpp2d_name(char const*);\n"
	             "#define CPP2D_ADD2(A, B) A##B\n"
	             "#define CPP2D_ADD(A, B) CPP2D_ADD2(A, B)\n";
	pp_.setPredefines(predefines);
}

void CPP2DPPHandling::InclusionDirective(
  SourceLocation,		//hash_loc,
  const Token&,			//include_token,
  StringRef file_name,
  bool,					//is_angled,
  CharSourceRange,		//filename_range,
  const FileEntry*,		//file,
  StringRef,			//search_path,
  StringRef,			//relative_path,
  const clang::Module*			//imported
)
{
	includes_in_file.insert(file_name);
}

//! Print arguments of the macro MI into a std::stringstream, for a normal macro declaration
auto print_macro_args(MacroInfo const* MI,
                      std::stringstream& new_macro,
                      char const* prefix = nullptr)
{
	new_macro << '(';
	bool first = true;
	for(IdentifierInfo const* arg : MI->params())
	{
		if(first)
			first = false;
		else
			new_macro << ", ";
		if(prefix)
			new_macro << prefix;
		new_macro << arg->getName().str();
	}
	new_macro << ')';
}

//! Print arguments of the macro MI into a std::stringstream,
//!  in a way they are compilable in C++, and convertible to D
void print_macro_args_expand(MacroInfo const* MI,
                             std::stringstream& new_macro,
                             std::string args)
{
	new_macro << '(';
	bool first = true;
	args.resize(MI->getNumParams(), 'n');
	auto arg_type_iter = std::begin(args);
	for(IdentifierInfo const* arg : MI->params())
	{
		if(first)
			first = false;
		else
			new_macro << ", ";
		switch(*arg_type_iter)
		{
		case 't':
			new_macro << "cpp2d_type<" << arg->getName().str() << ">()";
			break;
		case 'e':
			new_macro << arg->getName().str();
			break;
	case 'n': default:
			new_macro << "cpp2d_name(#" << arg->getName().str() << ")";
		}
		++arg_type_iter;
	}
	new_macro << ')';
}

//! Print the macro, as a C macro
std::string print_macro(MacroInfo const* MI)
{
	std::string new_macro;
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
	return new_macro;
}

//! Print the macro as a D mixin
std::string make_d_macro(MacroInfo const* MI, std::string const& name)
{
	std::set<std::string> arg_names;
	for(IdentifierInfo const* arg : MI->params())
		arg_names.insert(arg->getName());

	std::stringstream d_templ;
	d_templ << "template " + name;
	if(MI->isFunctionLike())
		print_macro_args(MI, d_templ, "string ");
	else
		d_templ << "()";
	d_templ << "\n{\n";
	d_templ << "    const char[] " + name + " = ";
	bool first = true;
	bool next_is_str = false;
	bool next_is_pasted = false;
	for(Token const& Tok : MI->tokens())
	{
		if(!first && Tok.hasLeadingSpace())
			d_templ << "\" \"";
		first = false;

		if(const char* Punc = tok::getPunctuatorSpelling(Tok.getKind()))
		{
			if(Punc == std::string("#"))
				next_is_str = true;
			else if(Punc == std::string("##"))
				next_is_pasted = true;
			else
				d_templ << std::string(1, '"') + Punc + '"';
		}
		else if(Tok.isLiteral() && Tok.getLiteralData())
		{
			std::string const literal =
			  CPP2DTools::replaceString(
			    std::string(Tok.getLiteralData(), Tok.getLength()), "\"", "\\\"");
			d_templ << '"' + literal + '"';
		}
		else if(auto* II = Tok.getIdentifierInfo())
		{
			std::string const identifierName =
			  CPP2DTools::replaceString(II->getName(), "\"", "\\\"");
			if(arg_names.count(identifierName))
			{
				if(next_is_str)
				{
					d_templ << "\"q{\" ~ " + identifierName + " ~ \"}\"";
					next_is_str = false;
				}
				else if(next_is_pasted)
				{
					d_templ << " ~ " + identifierName + " ~ ";
					next_is_pasted = false;
				}
				else
					d_templ << "\" \" ~ " + identifierName + " ~ \" \"";
			}
			else
				d_templ << '"' + identifierName + '"';
		}
		else
			d_templ << std::string(1, '"') + Tok.getName() + '"';
	}
	d_templ << ";\n";
	d_templ << "}\n";

	std::string d_templ_str = d_templ.str();
	size_t pos = std::string::npos;
	while((pos = d_templ_str.find("\"\"")) != std::string::npos)
		d_templ_str = d_templ_str.substr(0, pos) + d_templ_str.substr(pos + 2);
	return d_templ_str;
}

namespace
{
static std::string const MemFileSuffix = "_MemFileSuffix";
}

void CPP2DPPHandling::inject_macro(
  MacroDirective const* MD,
  std::string const& name,
  std::string const& new_macro)
{
	// TODO : Find a better way if it exists
	auto iter_inserted = new_macros.insert(new_macro);
	std::unique_ptr<MemoryBuffer> membuf =
	  MemoryBuffer::getMemBuffer(*iter_inserted.first, "<" + name + MemFileSuffix + ">");
	assert(membuf);
	FileID fileID = pp.getSourceManager().createFileID(
	                  std::move(membuf), clang::SrcMgr::C_User, 0, 0, MD->getLocation());

	pp.EnterSourceFile(fileID, pp.GetCurDirLookup(), MD->getMacroInfo()->getDefinitionEndLoc());

	char const* filename = CPP2DTools::getFile(sourceManager, MD->getLocation());
	if(CPP2DTools::checkFilename(modulename, filename))
		add_before_decl.insert(make_d_macro(MD->getMacroInfo(), name));
}

void CPP2DPPHandling::TransformMacroExpr(
  Token const& MacroNameTok,
  MacroDirective const* MD,
  CPP2DPPHandling::MacroInfo const& macro_options)
{
	clang::MacroInfo const* MI = MD->getMacroInfo();

	std::stringstream new_macro;
	new_macro << "\n#define " + macro_options.name;

	if(MI->isFunctionLike())
		print_macro_args(MI, new_macro);
	new_macro << " (\"CPP2D_MACRO_EXPR\", ((\"" + macro_options.name + "\", cpp2d_dummy_variadic";
	if(MI->isFunctionLike())
		print_macro_args_expand(MI, new_macro, macro_options.argType);
	else
		new_macro << "()";
	new_macro << "), ";
	if(macro_options.cppReplace.empty())
		new_macro << print_macro(MI);
	else
		new_macro << macro_options.cppReplace;
	new_macro << + "))\n";

	SourceLocation const location = MacroNameTok.getLocation();
	if(location.printToString(sourceManager).find(MemFileSuffix) != std::string::npos)
		return;

	pp.appendMacroDirective(MacroNameTok.getIdentifierInfo(), new UndefMacroDirective(location));

	inject_macro(MD, macro_options.name, new_macro.str());
}

void CPP2DPPHandling::TransformMacroStmt(
  Token const& MacroNameTok,
  MacroDirective const* MD,
  CPP2DPPHandling::MacroInfo const& macro_options)
{
	clang:: MacroInfo const* MI = MD->getMacroInfo();

	std::stringstream new_macro;
	new_macro << "\n#define " + macro_options.name;

	if(MI->isFunctionLike())
		print_macro_args(MI, new_macro);
	new_macro << " int CPP2D_ADD(CPP2D_MACRO_STMT, __COUNTER__) = (\""
	          << macro_options.name << "\", cpp2d_dummy_variadic";
	if(MI->isFunctionLike())
		print_macro_args_expand(MI, new_macro, macro_options.argType);
	else
		new_macro << "()";
	new_macro << ");\\\n";

	new_macro << print_macro(MI) << "\\\n";
	new_macro << "int CPP2D_ADD(CPP2D_MACRO_STMT_END, __COUNTER__) = 0;";

	SourceLocation const location = MacroNameTok.getLocation();
	if(location.printToString(sourceManager).find(MemFileSuffix) != std::string::npos)
		return;

	pp.appendMacroDirective(MacroNameTok.getIdentifierInfo(), new UndefMacroDirective(location));

	inject_macro(MD, macro_options.name, new_macro.str());
}


void CPP2DPPHandling::MacroDefined(const Token& MacroNameTok, const MacroDirective* MD)
{
	std::string const& name = MacroNameTok.getIdentifierInfo()->getName();
	clang::MacroInfo const* MI = MD->getMacroInfo();
	if(MI->isBuiltinMacro())
		return;
	auto macro_expr_iter = macro_expr.find(name);
	if(macro_expr_iter != macro_expr.end())
		TransformMacroExpr(MacroNameTok, MD, macro_expr_iter->second);
	else
	{
		auto macro_stmt_iter = macro_stmt.find(name);
		if(macro_stmt_iter != macro_stmt.end())
			TransformMacroStmt(MacroNameTok, MD, macro_stmt_iter->second);
	}
}

std::set<std::string> const& CPP2DPPHandling::getIncludes() const
{
	return includes_in_file;
}

std::set<std::string> const& CPP2DPPHandling::getInsertedBeforeDecls() const
{
	return add_before_decl;
}
