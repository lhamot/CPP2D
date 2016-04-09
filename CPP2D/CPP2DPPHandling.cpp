#include "CPP2DPPHandling.h"

#pragma warning(push, 0)
#pragma warning(disable, 4702)
#include <llvm/Support/Path.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#pragma warning(pop)

#include <sstream>

using namespace clang;
using namespace llvm;

std::string predefines;
std::set<std::string> new_macros;
std::set<std::string> new_macros_name;

extern cl::list<std::string> MacroAsExpr;
extern cl::list<std::string> MacroAsStmt;

CPP2DPPHandling::CPP2DPPHandling(Preprocessor& pp, StringRef inFile)
	: pp_(pp)
	, inFile_(inFile)
	, modulename_(llvm::sys::path::stem(inFile))
{
	auto split = [](std::string name)
	{
		std::string args;
		size_t const arg_pos = name.find("/");
		if(arg_pos != std::string::npos)
		{
			args = name.substr(arg_pos + 1);
			name = name.substr(0, arg_pos);
		}
		return std::make_pair(name, args);
	};
	for(std::string name : MacroAsExpr)
	{
		auto name_args = split(name);
		macro_expr.emplace(name_args.first, name_args.second);
	}
	for(std::string name : MacroAsStmt)
	{
		auto name_args = split(name);
		macro_stmt.emplace(name_args.first, name_args.second);
	}

	// TODO : Find a better way if it exists
	predefines = pp_.getPredefines();
	predefines += "\nint cpp2d_dummy_variadic(...);\n";
	predefines += "template<typename T> int cpp2d_type();\n";
	predefines += "int cpp2d_name(char const*);\n";
	predefines += "#define CPP2D_ADD2(A, B) A##B\n";
	predefines += "#define CPP2D_ADD(A, B) CPP2D_ADD2(A, B)\n";
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
  const Module*			//imported
)
{
	includes_in_file.insert(file_name);
}

auto print_macro_args(MacroInfo const* MI,
                      std::stringstream& new_macro,
                      char const* prefix = nullptr)
{
	new_macro << '(';
	bool first = true;
	for(IdentifierInfo const* arg : MI->args())
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

auto print_macro_args_expand(MacroInfo const* MI,
                             std::stringstream& new_macro,
                             std::string args)
{
	new_macro << '(';
	bool first = true;
	args.resize(MI->getNumArgs(), 'n');
	auto arg_type_iter = std::begin(args);
	for(IdentifierInfo const* arg : MI->args())
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

std::string expand_macro(MacroInfo const* MI)
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

std::string make_d_macro(MacroInfo const* MI, std::string const& name)
{
	std::set<std::string> arg_names;
	for(IdentifierInfo const* arg : MI->args())
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
			d_templ << '"' + std::string(Tok.getLiteralData(), Tok.getLength()) + '"';
		else if(auto* II = Tok.getIdentifierInfo())
		{
			if(arg_names.count(II->getName()))
			{
				if(next_is_str)
				{
					d_templ << "\"q{\" ~ " + std::string(II->getName()) + " ~ \"}\"";
					next_is_str = false;
				}
				else if(next_is_pasted)
				{
					d_templ << " ~ " + II->getName().str() + " ~ ";
					next_is_pasted = false;
				}
				else
					d_templ << "\" \" ~ " + II->getName().str() + " ~ \" \"";
			}
			else
				d_templ << '"' + II->getName().str() + '"';
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

void CPP2DPPHandling::inject_macro(
  MacroDirective const* MD,
  std::string const& name,
  std::string const& new_macro)
{
	// TODO : Find a better way if it exists
	auto iter_inserted = new_macros.insert(new_macro);
	std::unique_ptr<MemoryBuffer> membuf =
	  MemoryBuffer::getMemBuffer(*iter_inserted.first, "<" + name + "_macro_override>");
	FileID fileID = pp_.getSourceManager().createFileID(std::move(membuf));
	pp_.EnterSourceFile(fileID, pp_.GetCurDirLookup(), MD->getMacroInfo()->getDefinitionEndLoc());

	add_before_decl.insert(make_d_macro(MD->getMacroInfo(), name));
}

void CPP2DPPHandling::TransformMacroExpr(
  Token const& MacroNameTok,
  MacroDirective const* MD,
  std::string const& name,
  std::string const& args)
{
	MacroInfo const* MI = MD->getMacroInfo();

	pp_.appendMacroDirective(MacroNameTok.getIdentifierInfo(),
	                         new UndefMacroDirective(MacroNameTok.getLocation()));

	new_macros_name.insert(name);
	std::stringstream new_macro;
	new_macro << "\n#define " + name;

	if(MI->isFunctionLike())
		print_macro_args(MI, new_macro);
	new_macro << " (\"CPP2D_MACRO_EXPR\", ((\"" + name + "\", cpp2d_dummy_variadic";
	if(MI->isFunctionLike())
		print_macro_args_expand(MI, new_macro, args);
	else
		new_macro << "()";
	new_macro << "), " + expand_macro(MI) + "))\n";

	errs() << new_macro.str();

	inject_macro(MD, name, new_macro.str());
}

void CPP2DPPHandling::TransformMacroStmt(
  Token const& MacroNameTok,
  MacroDirective const* MD,
  std::string const& name,
  std::string const& args)
{
	MacroInfo const* MI = MD->getMacroInfo();

	pp_.appendMacroDirective(MacroNameTok.getIdentifierInfo(),
	                         new UndefMacroDirective(MacroNameTok.getLocation()));

	new_macros_name.insert(name);
	std::stringstream new_macro;
	new_macro << "\n#define " + name;

	if(MI->isFunctionLike())
		print_macro_args(MI, new_macro);
	new_macro << " int CPP2D_ADD(CPP2D_MACRO_STMT, __COUNTER__) = (\""
	          << name << "\", cpp2d_dummy_variadic";
	if(MI->isFunctionLike())
		print_macro_args_expand(MI, new_macro, args);
	else
		new_macro << "()";
	new_macro << ");\\\n";

	new_macro << expand_macro(MI) << "\\\n";
	new_macro << "int CPP2D_ADD(CPP2D_MACRO_STMT_END, __COUNTER__);";

	errs() << new_macro.str();

	inject_macro(MD, name, new_macro.str());
}


void CPP2DPPHandling::MacroDefined(const Token& MacroNameTok, const MacroDirective* MD)
{
	std::string const& name = MacroNameTok.getIdentifierInfo()->getName();
	MacroInfo const* MI = MD->getMacroInfo();
	if(new_macros_name.count(name) || MI->isBuiltinMacro())
		return;
	auto macro_expr_iter = macro_expr.find(name);
	auto macro_stmt_iter = macro_stmt.find(name);
	std::string new_macro;
	if(macro_expr_iter != macro_expr.end())
		TransformMacroExpr(MacroNameTok, MD, name, macro_expr_iter->second);
	else if(macro_stmt_iter != macro_stmt.end())
		TransformMacroStmt(MacroNameTok, MD, name, macro_stmt_iter->second);
}

void CPP2DPPHandling::MacroExpands(
  const clang::Token&, //MacroNameTok,
  const clang::MacroDefinition&, //MD,
  clang::SourceRange, //Range,
  const clang::MacroArgs* //Args
)
{
}