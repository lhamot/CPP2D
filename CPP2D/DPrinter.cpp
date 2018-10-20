//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "DPrinter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <ciso646>
#include <cstdio>
#include <regex>

#pragma warning(push, 0)
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/ConvertUTF.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroArgs.h>
#include <clang/AST/Comment.h>
#include <clang/AST/Expr.h>
#pragma warning(pop)

#include "MatchContainer.h"
#include "CPP2DTools.h"
#include "Spliter.h"
#include "CPP2DTools.h"

using namespace llvm;
using namespace clang;

std::vector<std::unique_ptr<std::stringstream> > outStack;

bool output_enabled = true;

std::stringstream& out()
{
	static std::stringstream empty_ss;
	empty_ss.str("");
	if(output_enabled)
		return *outStack.back();
	else
		return empty_ss;
}

void pushStream()
{
	outStack.emplace_back(std::make_unique<std::stringstream>());
}

std::string popStream()
{
	std::string const str = outStack.back()->str();
	outStack.pop_back();
	return str;
}

#define CHECK_LOC  if (checkFilename(Decl)) {} else return true

static std::map<std::string, std::string> type2type =
{
	{ "boost::optional", "std.typecons.Nullable" },
	{ "std::vector", "cpp_std.vector" },
	{ "std::set", "cpp_std.set" },
	{ "boost::shared_mutex", "core.sync.rwmutex.ReadWriteMutex" },
	{ "boost::mutex", "core.sync.mutex.Mutex" },
	{ "std::allocator", "cpp_std.allocator" },
	{ "time_t", "core.stdc.time.time_t" },
	{ "intptr_t", "core.stdc.stdint.intptr_t" },
	{ "int8_t", "core.stdc.stdint.int8_t" },
	{ "uint8_t", "core.stdc.stdint.uint8_t" },
	{ "int16_t", "core.stdc.stdint.int16_t" },
	{ "uint16_t", "core.stdc.stdint.uint16_t" },
	{ "int32_t", "core.stdc.stdint.int32_t" },
	{ "uint32_t", "core.stdc.stdint.uint32_t" },
	{ "int64_t", "core.stdc.stdint.int64_t" },
	{ "uint64_t", "core.stdc.stdint.uint64_t" },
	{ "size_t", "size_t" },
	{ "SafeInt", "std.experimental.safeint.SafeInt" },
	{ "RedBlackTree", "std.container.rbtree" },
	{ "std::string", "string" },
	{ "std::__cxx11::string", "string" },
	{ "std::ostream", "std.stdio.File" },
	{ "std::rand", "core.stdc.rand" },
	{ "::rand", "core.stdc.stdlib.rand" },
};


static const std::set<Decl::Kind> noSemiCommaDeclKind =
{
	Decl::Kind::CXXRecord,
	Decl::Kind::Record,
	Decl::Kind::ClassTemplate,
	Decl::Kind::Function,
	Decl::Kind::CXXConstructor,
	Decl::Kind::CXXDestructor,
	Decl::Kind::CXXConversion,
	Decl::Kind::CXXMethod,
	Decl::Kind::Namespace,
	Decl::Kind::NamespaceAlias,
	Decl::Kind::UsingDirective,
	Decl::Kind::Empty,
	Decl::Kind::Friend,
	Decl::Kind::FunctionTemplate,
	Decl::Kind::Enum,
};

static const std::set<Stmt::StmtClass> noSemiCommaStmtKind =
{
	Stmt::StmtClass::ForStmtClass,
	Stmt::StmtClass::IfStmtClass,
	Stmt::StmtClass::CXXForRangeStmtClass,
	Stmt::StmtClass::WhileStmtClass,
	Stmt::StmtClass::CompoundStmtClass,
	Stmt::StmtClass::CXXCatchStmtClass,
	Stmt::StmtClass::CXXTryStmtClass,
	Stmt::StmtClass::NullStmtClass,
	Stmt::StmtClass::SwitchStmtClass,
	//Stmt::StmtClass::DeclStmtClass,
};

bool needSemiComma(Decl* decl)
{
	auto const kind = decl->getKind();
	if (auto record = dyn_cast<RecordDecl>(decl))
		return !record->isCompleteDefinition();
	else if (auto tmp = dyn_cast<ClassTemplateDecl>(decl))
		return !tmp->hasBody();
	else
		return noSemiCommaDeclKind.count(kind) == 0;
}

bool needSemiComma(Stmt* stmt)
{
	if (auto* declStmt = dyn_cast<DeclStmt>(stmt))
	{
		if (declStmt->isSingleDecl()) //May be in for or catch
			return needSemiComma(declStmt->getSingleDecl());
		else
			return false;//needSemiComma(*(declStmt->children().end()--));
	}
	else
	{
		auto const kind = stmt->getStmtClass();
		return noSemiCommaStmtKind.count(kind) == 0;
	}
}

std::string mangleName(std::string const& name)
{
	if(name == "version")
		return "version_";
	if(name == "out")
		return "out_";
	if(name == "in")
		return "in_";
	if(name == "ref")
		return "ref_";
	if(name == "debug")
		return "debug_";
	if(name == "function")
		return "function_";
	if(name == "cast")
		return "cast_";
	if(name == "align")
		return "align_";
	else if(name == "Exception")
		return "Exception_";
	else
		return name;
}

static char const* AccessSpecifierStr[] =
{
	"public",
	"protected",
	"private",
	"private" // ...the special value "none" which means different things in different contexts.
	//  (from the clang doxy)
};

void DPrinter::setIncludes(std::set<std::string> const& includes)
{
	includesInFile = includes;
}

void DPrinter::includeFile(std::string const& inclFile, std::string const& typeName)
{
	if(isInMacro)
		return;
	// For each #include find in the cpp
	for(std::string include : includesInFile)
	{
		auto const pos = inclFile.find(include);
		// TODO : Use llvm::path
		// If the include is found in the file inclFile, import it
		if(pos != std::string::npos &&
		   pos == (inclFile.size() - include.size()) &&
		   (pos == 0 || inclFile[pos - 1] == '/' || inclFile[pos - 1] == '\\'))
		{
			static std::string const hExt = ".h";
			static std::string const hppExt = ".hpp";
			if(include.find(hExt) == include.size() - hExt.size())
				include = include.substr(0, include.size() - hExt.size());
			if(include.find(hppExt) == include.size() - hppExt.size())
				include = include.substr(0, include.size() - hppExt.size());
			std::transform(std::begin(include), std::end(include),
			               std::begin(include),
			[](char c) {return static_cast<char>(tolower(c)); });
			std::replace(std::begin(include), std::end(include), '/', '.');
			std::replace(std::begin(include), std::end(include), '\\', '.');
			std::replace(std::begin(include), std::end(include), '-', '_');
			externIncludes[include].insert(typeName);
			break;
		}
	}
}

void DPrinter::printDeclContext(DeclContext* DC)
{
	if(DC->isTranslationUnit()) return;
	if(DC->isFunctionOrMethod()) return;
	printDeclContext(DC->getParent());

	if(NamespaceDecl* NS = dyn_cast<NamespaceDecl>(DC))
		return;
	else if(ClassTemplateSpecializationDecl* Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC))
	{
		if(passDecl(Spec) == false)
		{
			auto* type = Spec->getTypeForDecl();
			QualType qtype = type->getCanonicalTypeInternal();
			TraverseType(qtype);
		}
		out() << ".";
	}
	else if(TagDecl* Tag = dyn_cast<TagDecl>(DC))
	{
		if(TypedefNameDecl* Typedef = Tag->getTypedefNameForAnonDecl())
			out() << Typedef->getIdentifier()->getName().str() << ".";
		else if(Tag->getIdentifier())
			out() << Tag->getIdentifier()->getName().str() << ".";
		else
			return;
	}
}

std::string DPrinter::printDeclName(NamedDecl* decl)
{
	NamedDecl* canDecl = nullptr;
	std::string const& name = decl->getNameAsString();
	if(name.empty())
		return std::string();
	std::string qualName = name;
	std::string result;
	if(DeclContext* ctx = decl->getDeclContext())
	{
		pushStream();
		printDeclContext(ctx);
		result = popStream();
		qualName = decl->getQualifiedNameAsString();
	}

	auto qualTypeToD = type2type.find(qualName);
	if(qualTypeToD != type2type.end())
	{
		//There is a convertion to D
		auto const& dQualType = qualTypeToD->second;
		auto const dotPos = dQualType.find_last_of('.');
		auto const module = dotPos == std::string::npos ?
		                    std::string() :
		                    dQualType.substr(0, dotPos);
		if(not module.empty())  //Need an import
			externIncludes[module].insert(qualName);
		if(dotPos == std::string::npos)
			return result + dQualType;
		else
			return result + dQualType.substr(dotPos + 1);
	}
	else
	{
		NamedDecl const* usedDecl = canDecl ? canDecl : decl;
		includeFile(CPP2DTools::getFile(Context->getSourceManager(), usedDecl), qualName);
		return result + mangleName(name);
	}
}

std::string getName(DeclarationName const& dn)
{
	std::string name = dn.getAsString();
	if(name.empty())
	{
		std::stringstream ss;
		ss << "var" << dn.getAsOpaqueInteger();
		name = ss.str();
	}
	return name;
}

void DPrinter::printCommentBefore(Decl* t)
{
	SourceManager& sm = Context->getSourceManager();
	const RawComment* rc = Context->getRawCommentForDeclNoCache(t);
	if(rc && not rc->isTrailingComment())
	{
		using namespace std;
		out() << std::endl << indentStr();
		string comment = rc->getRawText(sm).str();
		auto end = std::remove(std::begin(comment), std::end(comment), '\r');
		comment.erase(end, comment.end());
		out() << comment << std::endl << indentStr();
	}
	else
		out() << std::endl << indentStr();
}

void DPrinter::printCommentAfter(Decl* t)
{
	SourceManager& sm = Context->getSourceManager();
	const RawComment* rc = Context->getRawCommentForDeclNoCache(t);
	if(rc && rc->isTrailingComment())
		out() << '\t' << rc->getRawText(sm).str();
}

std::string DPrinter::trim(std::string const& s)
{
	auto const pos1 = s.find_first_not_of("\r\n\t ");
	auto const pos2 = s.find_last_not_of("\r\n\t ");
	return pos1 == std::string::npos ?
	       std::string() :
	       s.substr(pos1, (pos2 - pos1) + 1);
}

std::vector<std::string> DPrinter::split_lines(std::string const& instr)
{
	std::vector<std::string> result;
	auto prevIter = std::begin(instr);
	auto iter = std::begin(instr);
	do
	{
		iter = std::find(prevIter, std::end(instr), '\n');
		result.push_back(std::string(prevIter, iter));
		if (iter != std::end(instr))
		{
			result.back() += "\n";
			prevIter = iter + 1;
		}
	}
	while(iter != std::end(instr));
	return result;
}

bool DPrinter::printStmtComment(SourceLocation& locStart,
                                SourceLocation const& locEnd,
                                SourceLocation const& nextStart,
                                bool doIndent)
{
	if(locStart.isInvalid() || locEnd.isInvalid() || locStart.isMacroID() || locEnd.isMacroID())
	{
		locStart = nextStart;
		return false;
	}
	auto& sm = Context->getSourceManager();
	std::string comment =
	  Lexer::getSourceText(CharSourceRange(SourceRange(locStart, locEnd), true),
	                       sm,
	                       LangOptions()
	                      ).str();

	// Uniformize end of lines
	comment = std::regex_replace(comment, std::regex(R"(\r)"), std::string(""));

	// Extract comments
	enum State
	{
		StartOfLine,
		Line,
		Slash,
		MultilineComment,
		MultilineComment_star,
		SinglelineComment,
		Pragma,
		Pragma_antislash,
	};
	State state = StartOfLine;
	struct StringWithState
	{
		StringWithState(State s) :state(s) {}
		State state = Line;
		std::string str;
	};
	std::vector<StringWithState> comments;
	comments.emplace_back(StartOfLine);
	auto splitComment = [&](State newState)
	{
		if (comments.back().state == Pragma)
		{
			std::string& pragma = comments.back().str;
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#define\s+(\w+)\s+(\w+)(\s*)$)"), std::string("auto const $1 = $2;$3"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#define\s+(\w+)(\s*)$)"), std::string("version = $1;$2"));  // OK
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#ifdef\s*([^\z]*?[^\\])(\s*)$)"), std::string("version($1)\n{$2"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#ifndef\s*([^\z]*?[^\\])(\s*)$)"), std::string("version(!($1))\n{$2"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#if\s*([^\z]*?[^\\])(\s*)$)"), std::string("$1\n{$2"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#elif\s*([^\z]*?[^\\])(\s*)$)"), std::string("}\nelse $1\n{$2"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#else(\s*))"), std::string("}\nelse\n{$1"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#endif(\s*))"), std::string("}$1"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*(\#undef\s+\w+\s*$))"), std::string("//$1"));
			pragma = std::regex_replace(pragma, std::regex(R"(^\s*\#error\s*([^\z]*?[^\\])(\s*)$)"), std::string("static_assert(false, $1);$2"));
			pragma = std::regex_replace(pragma, std::regex(R"(defined(\(\w+\)))"), std::string("version$1"));
			if (pragma.find("#define") == 0)
				pragma = "//" + std::regex_replace(pragma, std::regex(R"((\\))"), std::string("\n//"));
		}
		else if (comments.back().state == MultilineComment)
			comments.back().str += '\n';
		comments.emplace_back(newState);
	};
	auto push = [&](char c)
	{
		comments.back().str.push_back(c);
	};
	for (char c : comment)
	{
		switch (state)
		{
		case StartOfLine:
		{
			switch (c)
			{
			case '/': state = Slash; break;
			case '#': state = Pragma; splitComment(Pragma); push(c); break;
			case ' ': state = StartOfLine; push(c); break;
			case '\t': state = StartOfLine; push(c); break;
			case '\n': state = StartOfLine; push(c); splitComment(StartOfLine); break;
			default: state = Line; push(c);
			}
			break;
		}
		case Line:
		{
			switch (c)
			{
			case '/': state = Slash; break;
			case '\n': state = StartOfLine; push(c); splitComment(StartOfLine); break;
			default: push(c);
			}
			break;
		}
		case Slash:
		{
			switch (c)
			{
			case '*': state = MultilineComment; splitComment(MultilineComment); push('/'); push(c); break;
			case '/': state = SinglelineComment; splitComment(SinglelineComment); push('/'); push(c); break;
			default: state = Line; push(c);
			}
			break;
		}
		case MultilineComment:
		{
			switch (c)
			{
			case '*': state = MultilineComment_star; break;
			default: push(c);
			}
			break;
		}
		case MultilineComment_star:
		{
			switch (c)
			{
			case '/': state = Line; push('*'); push(c); splitComment(Line); break;
			default: state = MultilineComment; push(c);
			}
			break;
		}
		case SinglelineComment:
		{
			switch (c)
			{
			case '\n': state = StartOfLine; push(c); splitComment(Line); break;
			default: push(c);
			}
			break;
		}
		case Pragma:
		{
			switch (c)
			{
			case '\n': state = StartOfLine; push(c); splitComment(Line); break;
			case '\\': state = Pragma_antislash; push(c); break;
			default: push(c);
			}
			break;
		}
		case Pragma_antislash:
		{
			state = Pragma;
		}
		}
	}

	if(not comments.empty())
		comments.erase(comments.begin());
	if (not comments.empty())
		comments.erase(comments.end() - 1);

	bool printedSomething = false;
	Spliter split(*this, "");
	if(not comments.empty())
	{
		for(StringWithState const& ss : comments)
		{
			std::string const& c = ss.str;
			size_t incPos = c.find("#include");
			if (not c.empty() && incPos == std::string::npos && ss.state != StartOfLine)
			{
				split.split();
				if (doIndent)
					out() << indentStr();
				out() << c;
				printedSomething = true;
			}
		}
	}
	locStart = nextStart;
	return printedSomething;
}

void DPrinter::printMacroArgs(CallExpr* macro_args)
{
	Spliter split(*this, ", ");
	for(Expr* arg : macro_args->arguments())
	{
		split.split();
		out() << "q{";
		if(auto* matTmpExpr = dyn_cast<MaterializeTemporaryExpr>(arg))
		{
			Stmt* tmpExpr = matTmpExpr->getTemporary();
			if(auto* callExpr = dyn_cast<CallExpr>(tmpExpr))
			{
				if(auto* impCast = dyn_cast<ImplicitCastExpr>(callExpr->getCallee()))
				{
					if(auto* func = dyn_cast<DeclRefExpr>(impCast->getSubExpr()))
					{
						std::string const func_name = func->getNameInfo().getAsString();
						if(func_name == "cpp2d_type")
						{
							TraverseTemplateArgument(func->getTemplateArgs()->getArgument());
						}
						else if(func_name == "cpp2d_name")
						{
							auto* impCast2 = dyn_cast<ImplicitCastExpr>(callExpr->getArg(0));
							auto* str = dyn_cast<clang::StringLiteral>(impCast2->getSubExpr());
							out() << str->getString().str();
						}
						else
							TraverseStmt(arg);
					}
					else
						TraverseStmt(arg);
				}
				else
					TraverseStmt(arg);
			}
			else
				TraverseStmt(arg);
		}
		else
			TraverseStmt(arg);
		out() << "}";
	}
}

void DPrinter::printStmtMacro(std::string const& varName, Expr* init)
{
	if (varName.find("CPP2D_MACRO_STMT_END") == 0)
		--isInMacro;
	else if(varName.find("CPP2D_MACRO_STMT") == 0)
	{
		auto get_binop = [](Expr * paren)
		{
			auto cleanups = dyn_cast<ExprWithCleanups>(paren);
			if (cleanups)
				paren = cleanups->getSubExpr();

			auto parenExpr = dyn_cast<ParenExpr>(paren);
			assert(parenExpr);
			auto binOp = dyn_cast<BinaryOperator>(parenExpr->getSubExpr());
			assert(binOp);
			return binOp;
		};
		BinaryOperator* name_and_args = get_binop(init);
		auto* macro_name = dyn_cast<clang::StringLiteral>(name_and_args->getLHS());
		auto* macro_args = dyn_cast<CallExpr>(name_and_args->getRHS());
		out() << "mixin(" << macro_name->getString().str() << "!(";
		printMacroArgs(macro_args);
		out() << "))";
		++isInMacro;
	}
}

static PrintingPolicy getPrintingPolicy()
{
	LangOptions lo;
	lo.CPlusPlus14 = true;
	lo.DelayedTemplateParsing = false;
	PrintingPolicy printingPolicy(lo);
	printingPolicy.ConstantArraySizeAsWritten = true;
	return printingPolicy;
}

clang::PrintingPolicy DPrinter::printingPolicy = getPrintingPolicy();

DPrinter::DPrinter(
  ASTContext* Context,
  MatchContainer const& receiver,
  StringRef file)
	: Context(Context)
	, receiver(receiver)
	, modulename(llvm::sys::path::stem(file))
{
}

std::string DPrinter::indentStr() const
{
	return std::string(indent * 4, ' '); //-V112
}

bool DPrinter::isA(CXXRecordDecl* decl, std::string const& baseName)
{
	std::string declName = decl->getQualifiedNameAsString();
	if(declName == baseName)
		return true;
	for(CXXBaseSpecifier const& baseSpec : decl->bases())
	{
		CXXRecordDecl* base = baseSpec.getType()->getAsCXXRecordDecl();
		assert(base);
		if(isA(base, baseName))
			return true;
	}
	for(CXXBaseSpecifier const& baseSpec : decl->vbases())
	{
		CXXRecordDecl* base = baseSpec.getType()->getAsCXXRecordDecl();
		assert(base);
		if(isA(base, baseName))
			return true;
	}
	return false;
}

bool DPrinter::TraverseTranslationUnitDecl(TranslationUnitDecl* Decl)
{
	if(passDecl(Decl)) return true;

	outStack.clear();
	outStack.emplace_back(std::make_unique<std::stringstream>());

	std::error_code ec;
	llvm::raw_fd_ostream file(StringRef(modulename + ".print.cpp"), ec, sys::fs::OpenFlags());
	Decl->print(file);

	SourceLocation locStart = Decl->getLocStart();

	std::ofstream file2(modulename + ".source.cpp");
	auto& sm = Context->getSourceManager();

	for(clang::Decl* c : Decl->decls())
	{
		std::string decl_str =
		  Lexer::getSourceText(CharSourceRange(c->getSourceRange(), true),
		                       sm,
		                       LangOptions()
		                      ).str();
		file2 << decl_str;

		if (CPP2DTools::checkFilename(Context->getSourceManager(), modulename, c))
		{
			pushStream();

			if (locStart.isInvalid())
				locStart = sm.getLocForStartOfFile(sm.getMainFileID());

			printStmtComment(locStart,
				c->getSourceRange().getBegin(),
				c->getSourceRange().getEnd(),
				true
			);

			TraverseDecl(c);
			std::string const decl = popStream();
			if (not decl.empty())
			{
				printCommentBefore(c);
				out() << indentStr() << decl;
				if (needSemiComma(c))
					out() << ';';
				printCommentAfter(c);
			}
			output_enabled = (isInMacro == 0);
		}
	}

	printStmtComment(locStart, sm.getLocForEndOfFile(sm.getMainFileID()), clang::SourceLocation(), true);

	return true;
}

bool DPrinter::TraverseTypedefDecl(TypedefDecl* Decl)
{
	if(passDecl(Decl)) return true;

	pushStream();
	printType(Decl->getUnderlyingType());

	std::string const rhs = popStream();
	std::string const lhs = mangleName(Decl->getNameAsString());
	if (lhs != rhs && rhs.empty() == false)
		out() << "alias " << lhs << " = " << rhs;
	return true;
}

bool DPrinter::TraverseTypeAliasDecl(TypeAliasDecl* Decl)
{
	if(passDecl(Decl)) return true;
	pushStream();
	printType(Decl->getUnderlyingType());

	std::string const rhs = popStream();
	std::string const lhs = mangleName(Decl->getNameAsString());
	if (lhs != rhs && rhs.empty() == false)
		out() << "alias " << lhs << " = " << rhs;
	return true;
}

bool DPrinter::TraverseTypeAliasTemplateDecl(TypeAliasTemplateDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "alias " << mangleName(Decl->getNameAsString());
	printTemplateParameterList(Decl->getTemplateParameters(), "");
	out() << " = ";
	printType(Decl->getTemplatedDecl()->getUnderlyingType());
	return true;
}

bool DPrinter::TraverseFieldDecl(FieldDecl* Decl)
{
	if(passDecl(Decl)) return true;
	std::string const varName = Decl->getNameAsString();
	if(varName.find("CPP2D_MACRO_STMT") == 0)
	{
		if (Decl->hasInClassInitializer())
		{
			printStmtMacro(varName, Decl->getInClassInitializer());
			return true;
		}
		else
			out() << varName << " // NO InClassInitializer! (2)" << std::endl;
	}

	if(passDecl(Decl)) return true;
	if(Decl->isMutable())
		out() << "/*mutable*/";
	if(Decl->isBitField())
	{
		out() << "\t";
		printType(Decl->getType());
		out() << ", \"" << mangleName(varName) << "\", ";
		TraverseStmt(Decl->getBitWidth());
		out() << ',';
		externIncludes["std.bitmanip"].insert("bitfields");
	}
	else
	{
		printType(Decl->getType());
		out() << " " << mangleName(Decl->getNameAsString());
	}
	QualType const type = Decl->getType();
	if(Decl->hasInClassInitializer())
	{
		out() << " = ";
		TraverseStmt(Decl->getInClassInitializer());
	}
	else if(!type.getTypePtr()->isPointerType() && getSemantic(Decl->getType()) == TypeOptions::Reference)
	{
		out() << " = new ";
		printType(Decl->getType());
	}
	return true;
}

bool DPrinter::TraverseDependentNameType(DependentNameType* Type)
{
	if(passType(Type)) return false;
	if(NestedNameSpecifier* nns = Type->getQualifier())
		TraverseNestedNameSpecifier(nns);
	out() << Type->getIdentifier()->getName().str();
	return true;
}

bool DPrinter::TraverseAttributedType(AttributedType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getEquivalentType());
	return true;
}

bool DPrinter::TraverseDecayedType(DecayedType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getOriginalType());
	return true;
}

bool DPrinter::TraverseElaboratedType(ElaboratedType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getNamedType());
	return true;
}

bool DPrinter::TraverseInjectedClassNameType(InjectedClassNameType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getInjectedSpecializationType());
	return true;
}

bool DPrinter::TraverseSubstTemplateTypeParmType(SubstTemplateTypeParmType* Type)
{
	if(passType(Type)) return false;
	return true;
}

bool DPrinter::TraverseNestedNameSpecifier(NestedNameSpecifier* NNS)
{
	NestedNameSpecifier::SpecifierKind const kind = NNS->getKind();
	switch(kind)
	{
	//case NestedNameSpecifier::Namespace:
	//case NestedNameSpecifier::NamespaceAlias:
	//case NestedNameSpecifier::Global:
	//case NestedNameSpecifier::Super:
	case NestedNameSpecifier::TypeSpec:
	case NestedNameSpecifier::TypeSpecWithTemplate:
		printType(QualType(NNS->getAsType(), 0));
		out() << ".";
		break;
	case NestedNameSpecifier::Identifier:
		if(NNS->getPrefix())
			TraverseNestedNameSpecifier(NNS->getPrefix());
		out() << NNS->getAsIdentifier()->getName().str() << ".";
		break;
	}
	return true;
}

void DPrinter::printTmpArgList(std::string const& tmpArgListStr)
{
	out() << "!(" << tmpArgListStr << ')';
}


bool DPrinter::customTypePrinter(NamedDecl* decl)
{
	if(decl == nullptr)
		return false;
	std::string typeName = decl->getQualifiedNameAsString();
	for(auto const& name_printer : receiver.customTypePrinters)
	{
		llvm::Regex RE(name_printer.first);
		if(RE.match("::" + typeName))
		{
			name_printer.second(*this, decl);// TemplateName().getAsTemplateDecl());
			return true;
		}
	}
	return false;
}

bool DPrinter::TraverseTemplateSpecializationType(TemplateSpecializationType* Type)
{
	if(passType(Type)) return false;

	if(customTypePrinter(Type->getAsTagDecl()))
		return true;

	if(isStdArray(Type->desugar()))
	{
		printTemplateArgument(Type->getArg(0));
		out() << '[';
		printTemplateArgument(Type->getArg(1));
		out() << ']';
		return true;
	}
	else if(isStdUnorderedMap(Type->desugar()))
	{
		printTemplateArgument(Type->getArg(1));
		out() << '[';
		printTemplateArgument(Type->getArg(0));
		out() << ']';
		return true;
	}
	out() << printDeclName(Type->getTemplateName().getAsTemplateDecl());
	auto const argNum = Type->getNumArgs();
	Spliter spliter(*this, ", ");
	pushStream();
	for(unsigned int i = 0; i < argNum; ++i)
	{
		spliter.split();
		printTemplateArgument(Type->getArg(i));
	}
	printTmpArgList(popStream());
	return true;
}

bool DPrinter::TraverseTypedefType(TypedefType* Type)
{
	if(passType(Type)) return false;
	if(customTypePrinter(Type->getDecl()))
		return true;

	out() << printDeclName(Type->getDecl());
	return true;
}

template<typename InitList, typename AddBeforeEnd>
void DPrinter::traverseCompoundStmtImpl(CompoundStmt* Stmt, InitList initList, AddBeforeEnd addBeforEnd)
{
	SourceLocation locStart = Stmt->getLBracLoc().getLocWithOffset(1);
	out() << "{" << std::endl;
	++indent;
	initList();
	for(auto child : Stmt->children())
	{
		printStmtComment(locStart,
		                 child->getLocStart(),
		                 child->getLocEnd(),
		                 true
		);
		out() << indentStr();
		TraverseStmt(child);
		if(needSemiComma(child))
			out() << ";";
		out() << std::endl;
		output_enabled = (isInMacro == 0);
	}
	printStmtComment(
		locStart, 
		Stmt->getRBracLoc(),
		clang::SourceLocation(),
		true
	);
	addBeforEnd();
	--indent;
	out() << indentStr();
	out() << "}";
	out() << std::endl;
	out() << indentStr();
}

bool DPrinter::TraverseCompoundStmt(CompoundStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	traverseCompoundStmtImpl(Stmt, [] {}, [] {});
	return true;
}

template<typename InitList>
void DPrinter::traverseCXXTryStmtImpl(CXXTryStmt* Stmt, InitList initList)
{
	out() << "try" << std::endl << indentStr();
	auto tryBlock = Stmt->getTryBlock();
	traverseCompoundStmtImpl(tryBlock, initList, [] {});
	auto handlerCount = Stmt->getNumHandlers();
	for(decltype(handlerCount) i = 0; i < handlerCount; ++i)
	{
		out() << std::endl << indentStr();
		TraverseStmt(Stmt->getHandler(i));
	}
}

bool DPrinter::TraverseCXXTryStmt(CXXTryStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	traverseCXXTryStmtImpl(Stmt, [] {});
	return true;
}

bool DPrinter::passDecl(Decl* decl)
{
	auto printer = receiver.getPrinter(decl);
	if(printer)
	{
		printer(*this, decl);
		return true;
	}
	else
		return false;
}

bool DPrinter::passStmt(Stmt* stmt)
{
	auto printer = receiver.getPrinter(stmt);
	if(printer)
	{
		printer(*this, stmt);
		return true;
	}
	else
		return false;
}

bool DPrinter::passType(clang::Type* type)
{
	auto printer = receiver.getPrinter(type);
	if(printer)
	{
		printer(*this, type);
		return true;
	}
	else
		return false;
}


bool DPrinter::TraverseNamespaceDecl(NamespaceDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "// -> module " << mangleName(Decl->getNameAsString()) << ';' << std::endl;
	for(auto decl : Decl->decls())
	{
		pushStream();
		TraverseDecl(decl);
		std::string const declstr = popStream();
		if(not declstr.empty())
		{
			printCommentBefore(decl);
			out() << indentStr() << declstr;
			if(needSemiComma(decl))
				out() << ';';
			printCommentAfter(decl);
			out() << std::endl << std::endl;
		}
		output_enabled = (isInMacro == 0);
	}
	out() << "// <- module " << mangleName(Decl->getNameAsString()) << " end" << std::endl;
	return true;
}

bool DPrinter::TraverseCXXCatchStmt(CXXCatchStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "catch";
	if(Stmt->getExceptionDecl())
	{
		out() << '(';
		catchedExceptNames.push(getName(Stmt->getExceptionDecl()->getDeclName()));
		traverseVarDeclImpl(Stmt->getExceptionDecl());
		out() << ')';
	}
	else
	{
		out() << "(Throwable ex)";
		catchedExceptNames.push("ex");
	}
	out() << std::endl;
	out() << indentStr();
	TraverseStmt(Stmt->getHandlerBlock());
	catchedExceptNames.pop();
	return true;
}

bool DPrinter::TraverseAccessSpecDecl(AccessSpecDecl* Decl)
{
	if(passDecl(Decl)) return true;
	return true;
}

void DPrinter::printBasesClass(CXXRecordDecl* decl)
{
	Spliter splitBase(*this, ", ");
	if(decl->getNumBases() + decl->getNumVBases() != 0)
	{
		pushStream();
		auto printBaseSpec = [&](CXXBaseSpecifier & base)
		{
			splitBase.split();
			TagDecl* tagDecl = base.getType()->getAsTagDecl();
			NamedDecl* named = tagDecl ? dyn_cast<NamedDecl>(tagDecl) : nullptr;
			if(named && named->getNameAsString() == "noncopyable")
				return;
			AccessSpecifier const as = base.getAccessSpecifier();
			if(as != AccessSpecifier::AS_public)
			{
				llvm::errs()
				    << "error : class " << decl->getNameAsString()
				    << " use of base class protection private and protected is no supported\n";
				out() << "/*" << AccessSpecifierStr[as] << "*/ ";
			}
			TraverseType(base.getType());
		};
		for(CXXBaseSpecifier& base : decl->bases())
			printBaseSpec(base);
		for(CXXBaseSpecifier& base : decl->vbases())
			printBaseSpec(base);
		std::string const bases = popStream();
		if(not bases.empty())
			out() << " : " << bases;
	}
}

bool DPrinter::TraverseCXXRecordDecl(CXXRecordDecl* decl)
{
	if(passDecl(decl)) return true;
	if(decl->isClass())
	{
		for(auto* ctor : decl->ctors())
		{
			if(ctor->isImplicit()
			   && ctor->isCopyConstructor()
			   && not ctor->isDeleted()
			   && not isA(decl, "std::exception"))
			{
				llvm::errs() << "error : class " << decl->getNameAsString() <<
				             " is copy constructible which is not dlang compatible.\n";
				break;
			}
		}
	}
	traverseCXXRecordDeclImpl(decl, [] {}, [this, decl] {printBasesClass(decl); });
	return true;
}

bool DPrinter::TraverseRecordDecl(RecordDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseCXXRecordDeclImpl(Decl, [] {}, [] {});
	return true;
}

template<typename TmpSpecFunc, typename PrintBasesClass>
void DPrinter::traverseCXXRecordDeclImpl(
  RecordDecl* decl,
  TmpSpecFunc traverseTmpSpecs,
  PrintBasesClass printBasesClass)
{
	if(decl->isImplicit())
		return;
	if(decl->isCompleteDefinition() == false && decl->getDefinition() != nullptr)
		return;

	const bool isClass = decl->isClass();
	char const* struct_class =
	  decl->isClass() ? "class" :
	  decl->isUnion() ? "union" :
	  "struct";
	TypedefNameDecl* typedefDecl = decl->getTypedefNameForAnonDecl();
	if (typedefDecl)
		out() << struct_class << " " << mangleName(typedefDecl->getNameAsString());
	else
		out() << struct_class << " " << mangleName(decl->getNameAsString());

	traverseTmpSpecs();
	if(decl->isCompleteDefinition() == false)
		return;
	printBasesClass();
	out() << std::endl << indentStr() << "{";
	++indent;

	auto isBitField = [this](Decl * decl2) -> int
	{
		if(FieldDecl* field = llvm::dyn_cast<FieldDecl>(decl2))
		{
			if(field->isBitField())
				return static_cast<int>(field->getBitWidthValue(*Context));
			else
				return - 1;
		}
		else
			return -1;
	};

	auto roundPow2 = [](int bit_count)
	{
		return
		  bit_count <= 0 ? 0 :
		  bit_count <= 8 ? 8 :
		  bit_count <= 16 ? 16 :
		  bit_count <= 32 ? 32 : //-V112
		  64;
	};

	int bit_count = 0;
	bool inBitField = false;
	AccessSpecifier access = AccessSpecifier::AS_public;
	for(Decl* decl2 : decl->decls())
	{
		pushStream();
		int const bc = isBitField(decl2);
		bool const nextIsBitField = (bc >= 0);
		if(nextIsBitField)
			bit_count += bc;
		else if(bit_count != 0)
			out() << "\tuint, \"\", " << (roundPow2(bit_count) - bit_count) << "));\n"
			      << indentStr();
		TraverseDecl(decl2);
		std::string const declstr = popStream();
		if(not declstr.empty())
		{
			AccessSpecifier newAccess = decl2->getAccess();
			if(newAccess == AccessSpecifier::AS_none)
				newAccess = AccessSpecifier::AS_public;
			if(newAccess == AccessSpecifier::AS_private)
			{
				// A private method can't be virtual in D => change them to protected
				if(auto* meth = dyn_cast<CXXMethodDecl>(decl2))
				{
					if(meth->isVirtual())
						newAccess = AccessSpecifier::AS_protected;
				}
			}
			if(newAccess != access && (isInMacro == 0))
			{
				--indent;
				out() << std::endl << indentStr() << AccessSpecifierStr[newAccess] << ":";
				++indent;
				access = newAccess;
			}
			printCommentBefore(decl2);
			if(inBitField == false && nextIsBitField && (isInMacro == 0))
				out() << "mixin(bitfields!(\n" << indentStr();
			out() << declstr;
			if(needSemiComma(decl2) && nextIsBitField == false)
				out() << ";";
			printCommentAfter(decl2);
		}
		inBitField = nextIsBitField;
		if(nextIsBitField == false)
			bit_count = 0;
		output_enabled = (isInMacro == 0);
	}
	if(inBitField)
		out() << "\n" << indentStr() << "\tuint, \"\", "
		      << (roundPow2(bit_count) - bit_count) << "));";
	out() << std::endl;

	//Print all free operator inside the class scope
	auto record_name = decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString();
	for(auto rng = receiver.freeOperator.equal_range(record_name);
	    rng.first != rng.second;
	    ++rng.first)
	{
		out() << indentStr();
		traverseFunctionDeclImpl(const_cast<FunctionDecl*>(rng.first->second), 0);
		out() << std::endl;
	}
	for(auto rng = receiver.freeOperatorRight.equal_range(record_name);
	    rng.first != rng.second;
	    ++rng.first)
	{
		out() << indentStr();
		traverseFunctionDeclImpl(const_cast<FunctionDecl*>(rng.first->second), 1);
		out() << std::endl;
	}

	// print the opCmd operator
	if(auto* cxxRecordDecl = dyn_cast<CXXRecordDecl>(decl))
	{
		ClassInfo const& classInfo = classInfoMap[cxxRecordDecl];
		for(auto && type_info : classInfoMap[cxxRecordDecl].relations)
		{
			clang::Type const* type = type_info.first;
			RelationInfo& info = type_info.second;
			if(info.hasOpLess and info.hasOpEqual)
			{
				out() << indentStr() << "int opCmp(ref in ";
				printType(type->getPointeeType());
				out() << " other)";
				if(portConst)
					out() << " const";
				out() << "\n";
				out() << indentStr() << "{\n";
				++indent;
				out() << indentStr() << "return _opLess(other) ? -1: ((this == other)? 0: 1);\n";
				--indent;
				out() << indentStr() << "}\n";
			}
		}

		if(classInfo.hasOpExclaim and not classInfo.hasBoolConv)
		{
			out() << indentStr() << "bool opCast(T : bool)()";
			if(portConst)
				out() << " const";
			out() << "\n";
			out() << indentStr() << "{\n";
			++indent;
			out() << indentStr() << "return !_opExclaim();\n";
			--indent;
			out() << indentStr() << "}\n";
		}
	}

	--indent;
	out() << indentStr() << "}";
}

void DPrinter::printTemplateParameterList(TemplateParameterList* tmpParams,
    std::string const& prevTmplParmsStr)
{
	out() << "(";
	Spliter spliter1(*this, ", ");
	if(prevTmplParmsStr.empty() == false)
	{
		spliter1.split();
		out() << prevTmplParmsStr;
	}
	for(unsigned int i = 0, size = tmpParams->size(); i != size; ++i)
	{
		spliter1.split();
		NamedDecl* param = tmpParams->getParam(i);
		inTemplateParamList = true;
		TraverseDecl(param);
		inTemplateParamList = false;
		// Print default template arguments
		if(auto* FTTP = dyn_cast<TemplateTypeParmDecl>(param))
		{
			if(FTTP->hasDefaultArgument())
			{
				out() << " = ";
				printType(FTTP->getDefaultArgument());
			}
		}
		else if(auto* FNTTP = dyn_cast<NonTypeTemplateParmDecl>(param))
		{
			if(FNTTP->hasDefaultArgument())
			{
				out() << " = ";
				TraverseStmt(FNTTP->getDefaultArgument());
			}
		}
		else if(auto* FTTTP = dyn_cast<TemplateTemplateParmDecl>(param))
		{
			if(FTTTP->hasDefaultArgument())
			{
				out() << " = ";
				printTemplateArgument(FTTTP->getDefaultArgument().getArgument());
			}
		}
	}
	out() << ')';
}

bool DPrinter::TraverseClassTemplateDecl(ClassTemplateDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseCXXRecordDeclImpl(Decl->getTemplatedDecl(), [Decl, this]
	{
		printTemplateParameterList(Decl->getTemplateParameters(), "");
	},
	[this, Decl] {printBasesClass(Decl->getTemplatedDecl()); });
	return true;
}

TemplateParameterList* DPrinter::getTemplateParameters(ClassTemplateSpecializationDecl*)
{
	return nullptr;
}

TemplateParameterList* DPrinter::getTemplateParameters(
  ClassTemplatePartialSpecializationDecl* Decl)
{
	return Decl->getTemplateParameters();
}

bool DPrinter::TraverseClassTemplatePartialSpecializationDecl(
  ClassTemplatePartialSpecializationDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseClassTemplateSpecializationDeclImpl(Decl, Decl->getTemplateArgsAsWritten());
	return true;
}

bool DPrinter::TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseClassTemplateSpecializationDeclImpl(Decl, nullptr);
	return true;
}

void DPrinter::printTemplateArgument(TemplateArgument const& ta)
{
	switch(ta.getKind())
	{
	case TemplateArgument::Null: break;
	case TemplateArgument::Declaration: TraverseDecl(ta.getAsDecl()); break;
	case TemplateArgument::Integral: out() << ta.getAsIntegral().toString(10); break;
	case TemplateArgument::NullPtr: out() << "null"; break;
	case TemplateArgument::Type: printType(ta.getAsType()); break;
	case TemplateArgument::Pack:
	{
		Spliter split(*this, ", ");
		for(TemplateArgument const& arg : ta.pack_elements())
			split.split(), printTemplateArgument(arg);
		break;
	}
	default: TraverseTemplateArgument(ta);
	}
}

void DPrinter::printTemplateSpec_TmpArgsAndParms(
  TemplateParameterList& primaryTmpParams,
  TemplateArgumentList const& tmpArgs,
  const ASTTemplateArgumentListInfo* tmpArgsInfo,
  TemplateParameterList* newTmpParams,
  std::string const& prevTmplParmsStr
)
{
	assert(tmpArgs.size() == primaryTmpParams.size());
	out() << '(';
	Spliter spliter2(*this, ", ");
	if(prevTmplParmsStr.empty() == false)
	{
		spliter2.split();
		out() << prevTmplParmsStr;
	}
	if(newTmpParams)
	{
		for(decltype(newTmpParams->size()) i = 0, size = newTmpParams->size(); i != size; ++i)
		{
			NamedDecl* parmDecl = newTmpParams->getParam(i);
			IdentifierInfo* info = parmDecl->getIdentifier();
			std::string name = info->getName().str() + "_";
			renamedIdentifiers[info] = name;
		}
	}

	auto printRedefinedTmp = [&](NamedDecl * tmpl, TemplateArgument const & tmplDef)
	{
		spliter2.split();
		renameIdentifiers = false;
		TraverseDecl(tmpl);
		renameIdentifiers = true;
		out() << " : ";
		printTemplateArgument(tmplDef);
	};
	if(tmpArgsInfo)
	{
		for(unsigned i = 0, size = tmpArgsInfo->NumTemplateArgs; i != size; ++i)
			printRedefinedTmp(primaryTmpParams.getParam(i), (*tmpArgsInfo)[i].getArgument());
	}
	else
	{
		for(decltype(tmpArgs.size()) i = 0, size = tmpArgs.size(); i != size; ++i)
			printRedefinedTmp(primaryTmpParams.getParam(i), tmpArgs.get(i));
	}
	if(newTmpParams)
	{
		for(decltype(newTmpParams->size()) i = 0, size = newTmpParams->size(); i != size; ++i)
		{
			spliter2.split();
			TraverseDecl(newTmpParams->getParam(i));
		}
	}
	out() << ')';
}

template<typename D>
void DPrinter::traverseClassTemplateSpecializationDeclImpl(
  D* Decl,
  const ASTTemplateArgumentListInfo* tmpArgsInfo
)
{
	if(Decl->getSpecializationKind() == TSK_ExplicitInstantiationDeclaration
	   || Decl->getSpecializationKind() == TSK_ExplicitInstantiationDefinition
	   || Decl->getSpecializationKind() == TSK_ImplicitInstantiation)
		return;

	TemplateParameterList* tmpParams = getTemplateParameters(Decl);
	if(tmpParams)
	{
		unsigned Depth = tmpParams->getDepth();
		for(decltype(tmpParams->size()) i = 0, size = tmpParams->size(); i != size; ++i)
			templateArgsStack[Depth][i] = tmpParams->getParam(i);
	}
	TemplateParameterList& specializedTmpParams =
	  *Decl->getSpecializedTemplate()->getTemplateParameters();
	TemplateArgumentList const& tmpArgs = Decl->getTemplateArgs();
	traverseCXXRecordDeclImpl(Decl, [&]
	{
		printTemplateSpec_TmpArgsAndParms(specializedTmpParams, tmpArgs, tmpArgsInfo, tmpParams, "");
	},
	[this, Decl] {printBasesClass(Decl); });
	if(tmpParams)
		templateArgsStack[tmpParams->getDepth()].clear();
}

bool DPrinter::TraverseCXXConversionDecl(CXXConversionDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseFunctionDeclImpl(Decl);
	return true;
}

bool DPrinter::TraverseCXXConstructorDecl(CXXConstructorDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseFunctionDeclImpl(Decl);
	return true;
}

bool DPrinter::TraverseCXXDestructorDecl(CXXDestructorDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseFunctionDeclImpl(Decl);
	return true;
}

bool DPrinter::TraverseCXXMethodDecl(CXXMethodDecl* Decl)
{
	if(passDecl(Decl)) return true;
	if(Decl->getLexicalParent() == Decl->getParent())
		traverseFunctionDeclImpl(Decl);
	return true;
}

bool DPrinter::TraversePredefinedExpr(PredefinedExpr* expr)
{
	if(passStmt(expr)) return true;
	out() << "__PRETTY_FUNCTION__";
	return true;
}

bool DPrinter::TraverseCXXDefaultArgExpr(CXXDefaultArgExpr* expr)
{
	if(passStmt(expr)) return true;
	TraverseStmt(expr->getExpr());
	return true;
}

bool DPrinter::TraverseCXXUnresolvedConstructExpr(CXXUnresolvedConstructExpr*  Expr)
{
	if(passStmt(Expr)) return true;
	printType(Expr->getTypeAsWritten());
	Spliter spliter(*this, ", ");
	out() << "(";
	for(decltype(Expr->arg_size()) i = 0; i < Expr->arg_size(); ++i)
	{
		auto arg = Expr->getArg(i);
		if(arg->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
		{
			spliter.split();
			TraverseStmt(arg);
		}
	}
	out() << ")";
	return true;
}

bool DPrinter::TraverseCXXForRangeStmt(CXXForRangeStmt*  Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "foreach(";
	refAccepted = true;
	inForRangeInit = true;
	traverseVarDeclImpl(dyn_cast<VarDecl>(Stmt->getLoopVarStmt()->getSingleDecl()));
	inForRangeInit = false;
	refAccepted = false;
	out() << "; ";
	Expr* rangeInit = Stmt->getRangeInit();
	TraverseStmt(rangeInit);
	if(TagDecl* rangeInitDecl = rangeInit->getType()->getAsTagDecl())
	{
		if(isStdUnorderedMap(QualType(rangeInitDecl->getTypeForDecl(), 0)))
			out() << ".byKeyValue";
	}

	out() << ")" << std::endl;
	traverseCompoundStmtOrNot(Stmt->getBody());
	return true;
}

bool DPrinter::TraverseDoStmt(DoStmt*  Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "do" << std::endl;
	traverseCompoundStmtOrNot(Stmt->getBody());
	out() << "while(";
	TraverseStmt(Stmt->getCond());
	out() << ")";
	return true;
}

bool DPrinter::TraverseSwitchStmt(SwitchStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "switch(";
	TraverseStmt(Stmt->getCond());
	out() << ")" << std::endl << indentStr();
	if (auto* body = dyn_cast<CompoundStmt>(Stmt->getBody()))
	{
		auto* last = dyn_cast<DefaultStmt>(body->body_back());
		if(last)
			traverseCompoundStmtImpl(body, [] {}, [] {});
		else
			traverseCompoundStmtImpl(
				body, [] {}, [this] {out() << indentStr() << "default: break;\n"; });
	}
	return true;
}

bool DPrinter::TraverseCaseStmt(CaseStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "case ";
	TraverseStmt(Stmt->getLHS());
	out() << ":" << std::endl;
	++indent;
	out() << indentStr();
	TraverseStmt(Stmt->getSubStmt());
	--indent;
	return true;
}

bool DPrinter::TraverseBreakStmt(BreakStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "break";
	return true;
}

bool DPrinter::TraverseContinueStmt(ContinueStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "continue";
	return true;
}

bool DPrinter::TraverseStaticAssertDecl(StaticAssertDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "static assert(";
	TraverseStmt(Decl->getAssertExpr());
	out() << ", ";
	TraverseStmt(Decl->getMessage());
	out() << ")";
	return true;
}

bool DPrinter::TraverseDefaultStmt(DefaultStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "default:" << std::endl;
	++indent;
	out() << indentStr();
	TraverseStmt(Stmt->getSubStmt());
	--indent;
	return true;
}

bool DPrinter::TraverseCXXDeleteExpr(CXXDeleteExpr* Expr)
{
	if(passStmt(Expr)) return true;
	TraverseStmt(Expr->getArgument());
	out() << " = null";
	return true;
}

bool DPrinter::TraverseCXXNewExpr(CXXNewExpr* Expr)
{
	if(passStmt(Expr)) return true;
	out() << "new ";
	if(Expr->isArray())
	{
		printType(Expr->getAllocatedType());
		out() << '[';
		TraverseStmt(Expr->getArraySize());
		out() << ']';
	}
	else
	{
		switch(Expr->getInitializationStyle())
		{
		case CXXNewExpr::NoInit:
			printType(Expr->getAllocatedType());
			break;
		case CXXNewExpr::CallInit:
			printType(Expr->getAllocatedType());
			out() << '(';
			TraverseStmt(const_cast<CXXConstructExpr*>(Expr->getConstructExpr()));
			out() << ')';
			break;
		case CXXNewExpr::ListInit:
			TraverseStmt(Expr->getInitializer());
			break;
		}
	}
	return true;
}

void DPrinter::printCXXConstructExprParams(CXXConstructExpr* Init)
{
	if(Init->getNumArgs() == 1)   //Handle Copy ctor
	{
		QualType recordType = Init->getType();
		recordType.addConst();
		if(recordType == Init->getArg(0)->getType())
		{
			TraverseStmt(Init->getArg(0));
			return;
		}
	}
	printType(Init->getType());
	out() << '(';
	Spliter spliter(*this, ", ");
	size_t counter = 0;
	TypeOptions::Semantic const sem = getSemantic(Init->getType());
	for(auto arg : Init->arguments())
	{
		if(arg->getStmtClass() == Stmt::StmtClass::CXXDefaultArgExprClass
		   && ((counter != 0) || sem != TypeOptions::Value))
			break;
		spliter.split();
		TraverseStmt(arg);
		++counter;
	}
	out() << ')';
}

bool DPrinter::TraverseCXXConstructExpr(CXXConstructExpr* Init)
{
	if(passStmt(Init)) return true;
	if(Init->isListInitialization() && !Init->isStdInitListInitialization())
		out() << '{';

	Spliter spliter(*this, ", ");
	size_t count = 0;
	for(unsigned i = 0, e = Init->getNumArgs(); i != e; ++i)
	{
		if(isa<CXXDefaultArgExpr>(Init->getArg(i)) && (count != 0))
			break; // Don't print any defaulted arguments

		spliter.split();
		TraverseStmt(Init->getArg(i));
		++count;
	}
	if(Init->isListInitialization() && !Init->isStdInitListInitialization())
		out() << '}';

	return true;
}

void DPrinter::printType(QualType const& type)
{
	if(type.getTypePtr()->getTypeClass() == clang::Type::TypeClass::Auto)
	{
		if(type.isConstQualified() && portConst)
			out() << "const ";
		TraverseType(type);
	}
	else
	{
		bool printConst = portConst || isa<BuiltinType>(type->getCanonicalTypeUnqualified());
		if(type.isConstQualified() && printConst)
			out() << "const(";
		TraverseType(type);
		if(type.isConstQualified() && printConst)
			out() << ')';
	}
}

template<typename T>
T* dynCastAcrossCleanup(Expr* expr)
{
	if(auto* cleanup = dyn_cast<ExprWithCleanups>(expr))
		expr = cleanup->getSubExpr();
	return dyn_cast<T>(expr);
}

bool DPrinter::TraverseConstructorInitializer(CXXCtorInitializer* Init)
{
	if(Init->isAnyMemberInitializer())
	{
		if(Init->getInit()->getStmtClass() == Stmt::StmtClass::CXXDefaultInitExprClass)
			return true;

		FieldDecl* fieldDecl = Init->getAnyMember();
		TypeOptions::Semantic const sem = getSemantic(fieldDecl->getType());
		out() << fieldDecl->getNameAsString();
		out() << " = ";
		if(sem == TypeOptions::Value)
		{
			Expr* init = Init->getInit();
			if(auto* parenListExpr = dyn_cast<ParenListExpr>(init))
			{
				if(parenListExpr->getNumExprs() > 1)
				{
					printType(fieldDecl->getType());
					out() << '(';
				}
				TraverseStmt(Init->getInit());
				if(parenListExpr->getNumExprs() > 1)
					out() << ')';
			}
			else if(auto* ctorExpr = dyn_cast<CXXConstructExpr>(init))
			{
				if(ctorExpr->getNumArgs() > 1)
				{
					printType(fieldDecl->getType());
					out() << '(';
				}
				TraverseStmt(Init->getInit());
				if(ctorExpr->getNumArgs() > 1)
					out() << ')';
			}
			else
				TraverseStmt(Init->getInit());
		}
		else
		{
			isThisFunctionUsefull = true;
			Expr* init = Init->getInit();
			if(auto* ctor = dynCastAcrossCleanup<CXXConstructExpr>(init))
			{
				if(ctor->getNumArgs() == 1)
				{
					QualType initType = ctor->getArg(0)->getType().getCanonicalType();
					QualType fieldType = fieldDecl->getType().getCanonicalType();
					initType.removeLocalConst();
					fieldType.removeLocalConst();
					if(fieldType == initType)
					{
						TraverseStmt(init);
						out() << ".dup()";
						return true;
					}
				}
				if(sem == TypeOptions::AssocArray)
				{
					if(ctor->getNumArgs() == 0 || isa<CXXDefaultArgExpr>(*ctor->arg_begin()))
						return true;
				}
			}
			if (fieldDecl->getType()->isPointerType())
				TraverseStmt(init);
			else
			{
				out() << "new ";
				printType(fieldDecl->getType());
				out() << '(';
				TraverseStmt(init);
				out() << ')';
			}
		}
	}
	else if(Init->isWritten())
	{
		assert(Init->isBaseInitializer());
		Expr* decl = Init->getInit();
		CXXConstructorDecl* ctorDecl = llvm::dyn_cast<CXXConstructExpr>(decl)->getConstructor();
		if (ctorDecl->isDefaulted() == false)
		{
			out() << "super(";
			TraverseStmt(Init->getInit());
			out() << ")";
		}
	}
	return true;
}

void DPrinter::startCtorBody(FunctionDecl*) {}

void DPrinter::startCtorBody(CXXConstructorDecl* Decl)
{
	auto ctor_init_count = Decl->getNumCtorInitializers();
	if(ctor_init_count != 0)
	{
		for(CXXCtorInitializer* init : Decl->inits())
		{
			pushStream();
			TraverseConstructorInitializer(init);
			std::string const initStr = popStream();
			if(initStr.empty() == false)
			{
				// If nothing to print, default init is enought.
				if(initStr.substr(initStr.size() - 2) != "= ")
				{
					out() << std::endl << indentStr();
					out() << initStr;
					out() << ";";
				}
			}
		}
	}
}

void DPrinter::printFuncEnd(FunctionDecl*) {}

void DPrinter::printFuncEnd(CXXMethodDecl* Decl)
{
	if(Decl->isConst() && portConst)
		out() << " const";
}

void DPrinter::printSpecialMethodAttribute(CXXMethodDecl* Decl)
{
	if(Decl->isStatic())
		out() << "static ";
	CXXRecordDecl* record = Decl->getParent();
	if(record->isClass())
	{
		if(Decl->isPure())
			out() << "abstract ";
		if(Decl->size_overridden_methods() != 0)
			out() << "override ";
		if(Decl->isVirtual() == false)
			out() << "final ";
	}
	else
	{
		if(Decl->isPure())
		{
			llvm::errs() << "struct " << record->getName()
			             << " has abstract function, which is forbiden.\n";
			out() << "abstract ";
		}
		if(Decl->isVirtual())
		{
			llvm::errs() << "struct " << record->getName()
			             << " has virtual function, which is forbiden.\n";
			out() << "virtual ";
		}
		if(Decl->size_overridden_methods() != 0)
			out() << "override ";
	}
}

bool DPrinter::printFuncBegin(CXXMethodDecl* Decl, std::string& tmpParams, int arg_become_this)
{
	if(not Decl->isPure() && Decl->getBody() == nullptr)
		return false;
	if(Decl->isImplicit())
		return false;
	if(Decl->isMoveAssignmentOperator())
		return false;
	if(Decl->isOverloadedOperator()
	   && Decl->getOverloadedOperator() == OverloadedOperatorKind::OO_ExclaimEqual)
		return false;
	printSpecialMethodAttribute(Decl);
	printFuncBegin((FunctionDecl*)Decl, tmpParams, arg_become_this);
	return true;
}

bool DPrinter::printFuncBegin(FunctionDecl* Decl, std::string& tmpParams, int arg_become_this)
{
	if(Decl->isImplicit())
		return false;
	if(Decl->isOverloadedOperator()
	   && Decl->getOverloadedOperator() == OverloadedOperatorKind::OO_ExclaimEqual)
		return false;
	std::string const name = Decl->getNameAsString();
	if(name == "cpp2d_dummy_variadic")
		return false;
	printType(Decl->getReturnType());
	out() << " ";
	if(Decl->isOverloadedOperator())
	{
		QualType arg1Type;
		QualType arg2Type;
		CXXRecordDecl* arg1Record = nullptr;
		CXXRecordDecl* arg2Record = nullptr;
		auto getRecordType = [](QualType qt)
		{
			if(auto const* lval = dyn_cast<LValueReferenceType>(qt.getTypePtr()))
			{
				return lval->getPointeeType()->getAsCXXRecordDecl();
			}
			else
			{
				return qt->getAsCXXRecordDecl();
			}
		};
		if(auto* methodDecl = dyn_cast<CXXMethodDecl>(Decl))
		{
			arg1Type = methodDecl->getThisType(*Context);
			arg1Record = methodDecl->getParent();
			if(methodDecl->getNumParams() > 0)
			{
				arg2Type = methodDecl->getParamDecl(0)->getType();
				arg2Record = getRecordType(arg2Type);
			}
		}
		else
		{
			if(Decl->getNumParams() > 0)
			{
				arg1Type = Decl->getParamDecl(0)->getType();
				arg1Record = getRecordType(arg1Type);
			}
			if(Decl->getNumParams() > 1)
			{
				arg2Type = Decl->getParamDecl(1)->getType();
				arg2Record = getRecordType(arg2Type);
			}
		}
		auto const nbArgs = (arg_become_this == -1 ? 1 : 0) + Decl->getNumParams();
		std::string const right = (arg_become_this == 1) ? "Right" : "";
		OverloadedOperatorKind const opKind = Decl->getOverloadedOperator();
		if(opKind == OverloadedOperatorKind::OO_EqualEqual)
		{
			out() << "opEquals" + right;
			if(arg1Record)
				classInfoMap[arg1Record].relations[arg2Type.getTypePtr()].hasOpEqual = true;
			if(arg2Record)
				classInfoMap[arg2Record].relations[arg1Type.getTypePtr()].hasOpEqual = true;
		}
		else if(opKind == OverloadedOperatorKind::OO_Exclaim)
		{
			out() << "_opExclaim" + right;
			if(arg1Record)
				classInfoMap[arg1Record].hasOpExclaim = true;
		}
		else if(opKind == OverloadedOperatorKind::OO_Call)
			out() << "opCall" + right;
		else if(opKind == OverloadedOperatorKind::OO_Subscript)
			out() << "opIndex" + right;
		else if(opKind == OverloadedOperatorKind::OO_Equal)
			out() << "opAssign" + right;
		else if(opKind == OverloadedOperatorKind::OO_Less)
		{
			out() << "_opLess" + right;
			if(arg1Record)
				classInfoMap[arg1Record].relations[arg2Type.getTypePtr()].hasOpLess = true;
		}
		else if(opKind == OverloadedOperatorKind::OO_LessEqual)
			out() << "_opLessEqual" + right;
		else if(opKind == OverloadedOperatorKind::OO_Greater)
			out() << "_opGreater" + right;
		else if(opKind == OverloadedOperatorKind::OO_GreaterEqual)
			out() << "_opGreaterEqual" + right;
		else if(opKind == OverloadedOperatorKind::OO_PlusPlus && nbArgs == 2)
			out() << "_opPostPlusplus" + right;
		else if(opKind == OverloadedOperatorKind::OO_MinusMinus && nbArgs == 2)
			out() << "_opPostMinusMinus" + right;
		else
		{
			std::string spelling = getOperatorSpelling(opKind);
			if(nbArgs == 1)
				out() << "opUnary" + right;
			else  // Two args
			{
				if(spelling.back() == '=')  //Handle self assign operators
				{
					out() << "opOpAssign";
					spelling.resize(spelling.size() - 1);
				}
				else
					out() << "opBinary" + right;
			}
			tmpParams = "string op: \"" + spelling + "\"";
		}
	}
	else
		out() << mangleName(name);
	return true;
}

bool DPrinter::printFuncBegin(CXXConversionDecl* Decl, std::string& tmpParams, int)
{
	printSpecialMethodAttribute(Decl);
	printType(Decl->getConversionType());
	out() << " opCast";
	pushStream();
	out() << "T : ";
	if(Decl->getConversionType().getAsString() == "bool")
		classInfoMap[Decl->getParent()].hasBoolConv = true;
	printType(Decl->getConversionType());
	tmpParams = popStream();
	return true;
}

bool DPrinter::printFuncBegin(CXXConstructorDecl* Decl,
                              std::string&,	//tmpParams
                              int			//arg_become_this = -1
                             )
{
	if(Decl->isMoveConstructor() || Decl->getBody() == nullptr)
		return false;

	CXXRecordDecl* record = Decl->getParent();
	if(record->isStruct() || record->isUnion())
	{
		if(Decl->isDefaultConstructor() && Decl->getNumParams() == 0)
		{
			if(Decl->isExplicit() && Decl->isDefaulted() == false)
			{
				llvm::errs() << "error : " << Decl->getNameAsString()
				             << " struct has an explicit default ctor.\n";
				llvm::errs() << "\tThis is illegal in D language.\n";
				llvm::errs() << "\tRemove it, default it or replace it by a factory method.\n";
			}
			return false; //If default struct ctor : don't print
		}
	}
	else
	{
		if(Decl->isImplicit() && !Decl->isDefaultConstructor())
			return false;
	}
	out() << "this";
	return true;
}

bool DPrinter::printFuncBegin(CXXDestructorDecl* decl,
                              std::string&,	//tmpParams,
                              int				//arg_become_this = -1
                             )
{
	if(decl->isImplicit() || decl->getBody() == nullptr)
		return false;
	//if(Decl->isPure() && !Decl->hasBody())
	//	return false; //ctor and dtor can't be abstract
	//else
	out() << "~this";
	return true;
}

template<typename Decl>
TypeOptions::Semantic getThisSemantic(Decl* decl, ASTContext& context)
{
	if(decl->isStatic())
		return TypeOptions::Reference;
	auto* recordPtrType = dyn_cast<clang::PointerType>(decl->getThisType(context));
	return DPrinter::getSemantic(recordPtrType->getPointeeType());
}

TypeOptions::Semantic getThisSemantic(FunctionDecl*, ASTContext&)
{
	return TypeOptions::Reference;
}

template<typename D>
void DPrinter::traverseFunctionDeclImpl(
  D* Decl,
  int arg_become_this)
{
	if(Decl->isDeleted())
		return;
	if(Decl->isImplicit() && Decl->getBody() == nullptr)
		return;

	if(Decl != Decl->getCanonicalDecl() &&
	   not(Decl->getTemplatedKind() == FunctionDecl::TK_FunctionTemplateSpecialization
	       && Decl->isThisDeclarationADefinition()))
		return;

	pushStream();
	refAccepted = true;
	std::string const name = Decl->getQualifiedNameAsString();
	bool const isMain = name == "main";
	std::string tmplParamsStr;
	if(printFuncBegin(Decl, tmplParamsStr, arg_become_this) == false)
	{
		refAccepted = false;
		popStream();
		return;
	}
	bool tmplPrinted = false;
	switch(Decl->getTemplatedKind())
	{
	case FunctionDecl::TK_MemberSpecialization:
	case FunctionDecl::TK_NonTemplate:
		break;
	case FunctionDecl::TK_FunctionTemplate:
		if(FunctionTemplateDecl* tDecl = Decl->getDescribedFunctionTemplate())
		{
			printTemplateParameterList(tDecl->getTemplateParameters(), tmplParamsStr);
			tmplPrinted = true;
		}
		break;
	case FunctionDecl::TK_FunctionTemplateSpecialization:
	case FunctionDecl::TK_DependentFunctionTemplateSpecialization:
		if(FunctionTemplateDecl* tDecl = Decl->getPrimaryTemplate())
		{
			TemplateParameterList* primaryTmpParams = tDecl->getTemplateParameters();
			TemplateArgumentList const* tmpArgs = Decl->getTemplateSpecializationArgs();
			assert(primaryTmpParams && tmpArgs);
			printTemplateSpec_TmpArgsAndParms(
			  *primaryTmpParams,
			  *tmpArgs,
			  Decl->getTemplateSpecializationArgsAsWritten(),
			  nullptr,
			  tmplParamsStr);
			tmplPrinted = true;
		}
		break;
	default: assert(false && "Inconststent clang::FunctionDecl::TemplatedKind");
	}
	if(not tmplPrinted and not tmplParamsStr.empty())
		out() << '(' << tmplParamsStr << ')';
	out() << "(";
	inFuncParams = true;
	bool isConstMethod = false;
	auto* ctorDecl = dyn_cast<CXXConstructorDecl>(Decl);
	bool const isCopyCtor = ctorDecl && ctorDecl->isCopyConstructor();
	TypeOptions::Semantic const sem = getThisSemantic(Decl, *Context);
	if(Decl->getNumParams() != 0)
	{
		TypeSourceInfo* declSourceInfo = Decl->getTypeSourceInfo();
		FunctionTypeLoc funcTypeLoc;
		SourceLocation locStart;

		auto isConst = [](QualType type)
		{
			if(auto* ref = dyn_cast<LValueReferenceType>(type.getTypePtr()))
				return ref->getPointeeType().isConstQualified();
			else
				return type.isConstQualified();
		};

		++indent;
		size_t index = 0;
		size_t const numParam =
		  Decl->getNumParams() +
		  (Decl->isVariadic() ? 1 : 0) +
		  ((arg_become_this == -1) ? 0 : -1);
		FunctionDecl* bodyDecl = Decl;
		for(FunctionDecl* decl : Decl->redecls())  //Print params of the function definition
			if(decl != bodyDecl)                    //  rather than the function declaration
			{
				bodyDecl = decl;
				declSourceInfo = bodyDecl->getTypeSourceInfo();
			}

		if(declSourceInfo)
		{
			TypeLoc declTypeLoc = declSourceInfo->getTypeLoc();
			TypeLoc::TypeLocClass tlClass = declTypeLoc.getTypeLocClass();
			if(tlClass == TypeLoc::TypeLocClass::FunctionProto)
			{
				funcTypeLoc = declTypeLoc.castAs<FunctionTypeLoc>();
				locStart = funcTypeLoc.getLParenLoc().getLocWithOffset(1);
			}
		}

		for(ParmVarDecl* decl : bodyDecl->parameters())
		{
			if (isMain && Decl->getNumParams() == 2 && decl == bodyDecl->parameters()[0])
			{	// Remove the first parameter (argc) of the main function
				locStart = decl->getLocEnd();
				++index;
				continue;
			}
			if (arg_become_this == static_cast<int>(index))
			{
				isConstMethod = isConst(decl->getType());
				locStart = decl->getLocEnd();
			}
			else
			{
				SourceLocation endLoc = decl->getLocEnd();
				if (numParam != 1)
				{
					bool isComment = printStmtComment(locStart,
						decl->getLocStart(),
						endLoc);
					if(not isComment)
						out() << std::endl;
					out() << indentStr();
				}
				else
					locStart = endLoc;
				if(isCopyCtor && sem == TypeOptions::Value)
					out() << "this";
				else
				{
					if(index == 0
					   && sem == TypeOptions::Value
					   && (ctorDecl != nullptr))
						printDefaultValue = false;
					TraverseDecl(decl);
					printDefaultValue = true;
				}
				if(index < numParam - 1)
					out() << ", ";
			}
			++index;
		}
		if(Decl->isVariadic())
		{
			if(numParam != 1)
				out() << "\n" << indentStr();
			out() << "...";
			addExternInclude("core.vararg", "...");
			locStart = funcTypeLoc.getRParenLoc(); // the dots doesn't have Decl, but we have to go foreward
		}
		if (funcTypeLoc.isNull() == false)
		{
			bool isComment = printStmtComment(locStart, funcTypeLoc.getRParenLoc());
			if (not isComment)
				out() << std::endl;
		}
		--indent;
	}
	if(Decl->getNumParams() != 0)
		out() << indentStr() << ')';
	else
		out() << ')';
	if(isConstMethod && portConst)
		out() << " const";
	printFuncEnd(Decl);
	refAccepted = false;
	inFuncParams = false;
	isThisFunctionUsefull = false;
	if(Stmt* body = Decl->getBody())
	{
		//Stmt* body = Decl->getBody();
		out() << std::endl << std::flush;
		if(isCopyCtor && sem == TypeOptions::Value)
			arg_become_this = 0;
		auto alias_this = [Decl, arg_become_this, this]
		{
			if(arg_become_this >= 0)
			{
				ParmVarDecl* param = *(Decl->param_begin() + arg_become_this);
				std::string const this_name = getName(param->getDeclName());
				if(this_name.empty() == false)
					out() << indentStr() << "alias " << this_name << " = this;" << std::endl;
			}
		};
		if(body->getStmtClass() == Stmt::CXXTryStmtClass)
		{
			out() << indentStr() << '{' << std::endl;
			++indent;
			out() << indentStr();
			traverseCXXTryStmtImpl(static_cast<CXXTryStmt*>(body),
			                       [&] {alias_this(); startCtorBody(Decl); });
			out() << std::endl;
			--indent;
			out() << indentStr() << '}';
		}
		else
		{
			out() << indentStr();
			assert(body->getStmtClass() == Stmt::CompoundStmtClass);
			traverseCompoundStmtImpl(static_cast<CompoundStmt*>(body),
			                         [&] {alias_this(); startCtorBody(Decl); },
				                     [] {});
		}
	}
	else
		out() << ";";
	std::string printedFunction = popStream();
	if(not Decl->isImplicit() || isThisFunctionUsefull)
		out() << printedFunction;
	return;
}

bool DPrinter::TraverseUsingDecl(UsingDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "//using " << Decl->getNameAsString();
	return true;
}

bool DPrinter::TraverseFunctionDecl(FunctionDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseFunctionDeclImpl(Decl);
	return true;
}

bool DPrinter::TraverseUsingDirectiveDecl(UsingDirectiveDecl* Decl)
{
	if(passDecl(Decl)) return true;
	return true;
}


bool DPrinter::TraverseFunctionTemplateDecl(FunctionTemplateDecl* Decl)
{
	if(passDecl(Decl)) return true;
	FunctionDecl* FDecl = Decl->getTemplatedDecl();
	switch(FDecl->getKind())
	{
	case Decl::Function:
		traverseFunctionDeclImpl(FDecl);
		return true;
	case Decl::CXXMethod:
		traverseFunctionDeclImpl(llvm::cast<CXXMethodDecl>(FDecl));
		return true;
	case Decl::CXXConstructor:
		traverseFunctionDeclImpl(llvm::cast<CXXConstructorDecl>(FDecl));
		return true;
	case Decl::CXXConversion:
		traverseFunctionDeclImpl(llvm::cast<CXXConversionDecl>(FDecl));
		return true;
	case Decl::CXXDestructor:
		traverseFunctionDeclImpl(llvm::cast<CXXDestructorDecl>(FDecl));
		return true;
	default: assert(false && "Inconsistent FunctionDecl kind in FunctionTemplateDecl");
	}
	return true;
}

bool DPrinter::TraverseBuiltinType(BuiltinType* Type)
{
	if(passType(Type)) return false;
	out() << [this, Type]
	{
		BuiltinType::Kind k = Type->getKind();
		switch(k)
		{
		case BuiltinType::Void: return "void";
		case BuiltinType::Bool: return "bool";
		case BuiltinType::Char_S: return "char";
		case BuiltinType::Char_U: return "char";
		case BuiltinType::SChar: return "char";
		case BuiltinType::Short: return "short";
		case BuiltinType::Int: return "int";
		case BuiltinType::Long: return "long";
		case BuiltinType::LongLong: return "long";
		case BuiltinType::Int128: return "cent";
		case BuiltinType::UChar: return "ubyte";
		case BuiltinType::UShort: return "ushort";
		case BuiltinType::UInt: return "uint";
		case BuiltinType::ULong: return "ulong";
		case BuiltinType::ULongLong: return "ulong";
		case BuiltinType::UInt128: return "ucent";
		case BuiltinType::Half: return "half";
		case BuiltinType::Float: return "float";
		case BuiltinType::Double: return "double";
		case BuiltinType::LongDouble: return "real";
		case BuiltinType::WChar_S:
		case BuiltinType::WChar_U: return "wchar";
		case BuiltinType::Char16: return "wchar";
		case BuiltinType::Char32: return "dchar";
		case BuiltinType::NullPtr: return "nullptr_t";
		case BuiltinType::Overload: return "<overloaded function type>";
		case BuiltinType::BoundMember: return "<bound member function type>";
		case BuiltinType::PseudoObject: return "<pseudo-object type>";
		case BuiltinType::Dependent: return "<dependent type>";
		case BuiltinType::UnknownAny: return "<unknown type>";
		case BuiltinType::ARCUnbridgedCast: return "<ARC unbridged cast type>";
		case BuiltinType::BuiltinFn: return "<builtin fn type>";
		case BuiltinType::ObjCId: return "id";
		case BuiltinType::ObjCClass: return "Class";
		case BuiltinType::ObjCSel: return "SEL";
		default: return Type->getNameAsCString(printingPolicy);
		}
	}();
	return true;
}

TypeOptions::Semantic DPrinter::getSemantic(QualType qt)
{
	clang::Type const* type = qt.getTypePtr();
	std::string empty;
	raw_string_ostream os(empty);
	qt.getCanonicalType().getUnqualifiedType().print(os, printingPolicy);
	std::string const name = os.str();
	// TODO : Externalize the semantic customization
	if(name.find("class SafeInt<") == 0)
		return TypeOptions::Value;
	if(isStdArray(qt))
		return TypeOptions::Value;
	if(name.find("class std::basic_string<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::__cxx11::basic_string<") == 0)
		return TypeOptions::Value;
	if(name.find("class boost::optional<") == 0)
		return TypeOptions::Value;
	if(name.find("class boost::property_tree::basic_ptree<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::vector<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::shared_ptr<") == 0)
		return TypeOptions::Value;
	if(name.find("class boost::scoped_ptr<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::unique_ptr<") == 0)
		return TypeOptions::Value;
	if(isStdUnorderedMap(qt))
		return TypeOptions::AssocArray;
	if(name.find("class std::set<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::unordered_set<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::map<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::multiset<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::unordered_multiset<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::multimap<") == 0)
		return TypeOptions::Value;
	if(name.find("class std::unordered_multimap<") == 0)
		return TypeOptions::Value;

	for(auto& nvp : Options::getInstance().types)
	{
		if(name.find(nvp.first) == 0)
			return nvp.second.semantic;
	}

	if (auto *pt = dyn_cast<clang::PointerType>(type))
		return getSemantic(pt->getPointeeType());
	else
		return (type->isClassType() || type->isFunctionType()) ? 
			TypeOptions::Reference : TypeOptions::Value;
}

bool DPrinter::isPointer(QualType const& type)
{
	if(type->isPointerType())
		return true;

	QualType const rawType = type.isCanonical() ?
	                         type :
	                         type.getCanonicalType();
	std::string const name = rawType.getAsString();
	static std::string const arrayNames[] =
	{
		"class std::shared_ptr<",
		"class std::unique_ptr<",
		"struct std::shared_ptr<",
		"struct std::unique_ptr<",
		"class boost::shared_ptr<",
		"class boost::scoped_ptr<",
		"struct boost::shared_ptr<",
		"struct boost::scoped_ptr<",
	};
	return std::any_of(std::begin(arrayNames), std::end(arrayNames), [&](auto && arrayName)
	{
		return name.find(arrayName) == 0;
	});
}


template<typename PType>
void DPrinter::traversePointerTypeImpl(PType* Type)
{
	QualType const pointee = Type->getPointeeType();
	clang::Type::TypeClass const tc = pointee->getTypeClass();
	if(tc == clang::Type::Paren)  //function pointer do not need '*'
	{
		auto innerType = static_cast<ParenType const*>(pointee.getTypePtr())->getInnerType();
		if(innerType->getTypeClass() == clang::Type::FunctionProto)
		{
			TraverseType(innerType);
			return;
		}
	}
	printType(pointee);
	out() << ((getSemantic(pointee) == TypeOptions::Value) ? "[]" : ""); //'*';
}

bool DPrinter::TraverseMemberPointerType(MemberPointerType* Type)
{
	if(passType(Type)) return false;
	traversePointerTypeImpl(Type);
	return true;
}
bool DPrinter::TraversePointerType(clang::PointerType* Type)
{
	if(passType(Type)) return false;
	traversePointerTypeImpl(Type);
	return true;
}

bool DPrinter::TraverseCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr* Expr)
{
	if(passStmt(Expr)) return true;
	out() << "null";
	return true;
}

bool DPrinter::TraverseEnumConstantDecl(EnumConstantDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << mangleName(Decl->getNameAsString());
	if(Decl->getInitExpr())
	{
		out() << " = ";
		TraverseStmt(Decl->getInitExpr());
	}
	return true;
}

bool DPrinter::TraverseEnumDecl(EnumDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "enum";
	if (Decl->getDeclName()) {
		out() << " " << mangleName(Decl->getNameAsString());
	}
	if(Decl->isFixed())
	{
		out() << " : ";
		TraverseType(Decl->getIntegerType());
	}
	out() << std::endl << indentStr() << "{" << std::endl;
	++indent;
	size_t count = 0;
	for(auto e : Decl->enumerators())
	{
		++count;
		out() << indentStr();
		TraverseDecl(e);
		out() << "," << std::endl;
	}
	if(count == 0)
		out() << indentStr() << "Default" << std::endl;
	--indent;
	out() << indentStr() << "}";
	return true;
}

bool DPrinter::TraverseEnumType(EnumType* Type)
{
	if(passType(Type)) return false;
	out() << printDeclName(Type->getDecl());
	return true;
}

bool DPrinter::TraverseIntegerLiteral(IntegerLiteral* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << Stmt->getValue().toString(10, true);
	return true;
}

bool DPrinter::TraverseDecltypeType(DecltypeType* Type)
{
	if(passType(Type)) return false;
	out() << "typeof(";
	TraverseStmt(Type->getUnderlyingExpr());
	out() << ')';
	return true;
}

bool DPrinter::TraverseAutoType(AutoType* Type)
{
	if(passType(Type)) return false;
	if(not inForRangeInit)
		out() << "auto";
	return true;
}

bool DPrinter::TraverseLinkageSpecDecl(LinkageSpecDecl* Decl)
{
	if(passDecl(Decl)) return true;
	switch(Decl->getLanguage())
	{
	case LinkageSpecDecl::LanguageIDs::lang_c: out() << "extern (C) "; break;
	case LinkageSpecDecl::LanguageIDs::lang_cxx: out() << "extern (C++) "; break;
	default: assert(false && "Inconsistant LinkageSpecDecl::LanguageIDs");
	}
	DeclContext* declContext = LinkageSpecDecl::castToDeclContext(Decl);;
	if(Decl->hasBraces())
	{
		out() << "\n" << indentStr() << "{\n";
		++indent;
		for(auto* decl : declContext->decls())
		{
			out() << indentStr();
			TraverseDecl(decl);
			if(needSemiComma(decl))
				out() << ";";
			out() << "\n";
		}
		--indent;
		out() << indentStr() << "}";
	}
	else
		TraverseDecl(*declContext->decls_begin());
	return true;
}

bool DPrinter::TraverseFriendDecl(FriendDecl* Decl)
{
	if(passDecl(Decl)) return true;
	out() << "//friend ";
	if(Decl->getFriendType())
		TraverseType(Decl->getFriendType()->getType());
	else
		TraverseDecl(Decl->getFriendDecl());
	return true;
}

bool DPrinter::TraverseParmVarDecl(ParmVarDecl* Decl)
{
	if(passDecl(Decl)) return true;
	printType(Decl->getType());
	std::string const name = getName(Decl->getDeclName());//getNameAsString();
	if(name.empty() == false)
		out() <<  " " << mangleName(name);
	if(Decl->hasDefaultArg())
	{
		if(not printDefaultValue)
			out() << "/*";
		out() << " = ";
		TraverseStmt(
		  Decl->hasUninstantiatedDefaultArg() ?
		  Decl->getUninstantiatedDefaultArg() :
		  Decl->getDefaultArg());
		if(not printDefaultValue)
			out() << "*/";
	}
	return true;
}

bool DPrinter::TraverseRValueReferenceType(RValueReferenceType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getPointeeType());
	out() << "/*&&*/";
	return true;
}

bool DPrinter::TraverseLValueReferenceType(LValueReferenceType* Type)
{
	if(passType(Type)) return false;
	if(refAccepted)
	{
		if(getSemantic(Type->getPointeeType()) == TypeOptions::Value)
		{
			if(inFuncParams)
			{
				// In D, we can't take a rvalue by const ref. So we need to pass by copy.
				// (But the copy will be elided when possible)
				if(Type->getPointeeType().isConstant(*Context) == false)
					out() << "ref ";
			}
			else
				out() << "ref ";
		}
		printType(Type->getPointeeType());
	}
	else
	{
		QualType const pt = Type->getPointeeType();
		if(getSemantic(Type->getPointeeType()) == TypeOptions::Value)
		{
			out() << "Ref!(";
			if(auto* at = dyn_cast<AutoType>(pt))
				printType(at->getDeducedType());
			else
				printType(pt);
			out() << ")";
		}
		else
			printType(pt);
	}
	return true;
}

bool DPrinter::TraverseTemplateTypeParmType(TemplateTypeParmType* Type)
{
	if(passType(Type)) return false;
	if(Type->getDecl())
		TraverseDecl(Type->getDecl());
	else
	{
		IdentifierInfo* identifier = Type->getIdentifier();
		if(identifier == nullptr)
		{
			if(static_cast<size_t>(Type->getDepth()) >= templateArgsStack.size())
				out() << "/* getDepth : " << Type->getDepth() << "*/";
			else if(static_cast<size_t>(Type->getIndex()) >= templateArgsStack[Type->getDepth()].size())
				out() << "/* getIndex : " << Type->getIndex() << "*/";
			else
			{
				auto param = templateArgsStack[Type->getDepth()][Type->getIndex()];
				identifier = param->getIdentifier();
				if(identifier == nullptr)
					TraverseDecl(param);
			}
		}
		auto iter = renamedIdentifiers.find(identifier);
		if(iter != renamedIdentifiers.end())
			out() << iter->second;
		else if(identifier != nullptr)
			out() << identifier->getName().str();
		else
			out() << "cant_find_name";
	}
	return true;
}

bool DPrinter::TraversePackExpansionType(clang::PackExpansionType* type)
{
	printType(type->getPattern());
	return true;
}

bool DPrinter::TraversePackExpansionExpr(clang::PackExpansionExpr* expr)
{
	TraverseStmt(expr->getPattern());
	return true;
}

bool DPrinter::TraverseTemplateTypeParmDecl(TemplateTypeParmDecl* Decl)
{
	if(passDecl(Decl)) return true;
	IdentifierInfo* identifier = Decl->getIdentifier();
	if(identifier)
	{
		auto iter = renamedIdentifiers.find(identifier);
		if(renameIdentifiers && iter != renamedIdentifiers.end())
			out() << iter->second;
		else
			out() << identifier->getName().str();
	}
	if(Decl->isParameterPack() && inTemplateParamList)
		out() << "...";

	// A template type without name is a auto param of a lambda
	// Add "else" to handle it
	return true;
}

bool DPrinter::TraverseNonTypeTemplateParmDecl(NonTypeTemplateParmDecl* Decl)
{
	if(passDecl(Decl)) return true;
	printType(Decl->getType());
	out() << " ";
	IdentifierInfo* identifier = Decl->getIdentifier();
	if(identifier)
		out() << mangleName(identifier->getName());
	return true;
}

bool DPrinter::TraverseDeclStmt(DeclStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	if(Stmt->isSingleDecl()) //May be in for or catch
		TraverseDecl(Stmt->getSingleDecl());
	else
	{
		// Better to split multi line decl, but in for init, we can't do it
		if(splitMultiLineDecl)
		{
			int count = 0;
			for(auto d : Stmt->decls())
			{
				pushStream();
				TraverseDecl(d);
				std::string const line = popStream();
				if (line.empty() == false)
				{
					if (count != 0)
						out() << "\n" << indentStr();
					++count;
					out() << line;
					if (needSemiComma(d))
						out() << ";";
				}
			}
		}
		else
		{
			Spliter split(*this, ", ");
			for(auto d : Stmt->decls())
			{
				doPrintType = split.first;
				split.split();
				TraverseDecl(d);
				if(isa<RecordDecl>(d))
				{
					out() << "\n" << indentStr();
					split.first = true;
				}
				doPrintType = true;
			}
		}
	}
	return true;
}

bool DPrinter::TraverseNamespaceAliasDecl(NamespaceAliasDecl* Decl)
{
	if(passDecl(Decl)) return true;
	return true;
}

bool DPrinter::TraverseReturnStmt(ReturnStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "return";
	if(Stmt->getRetValue())
	{
		out() << " ";
		TraverseStmt(Stmt->getRetValue());
	}
	return true;
	out() << "return";
	if(Stmt->getRetValue())
	{
		out() << ' ';
		TraverseStmt(Stmt->getRetValue());
	}
	return true;
}

bool DPrinter::TraverseCXXOperatorCallExpr(CXXOperatorCallExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	auto const numArgs = Stmt->getNumArgs();
	const OverloadedOperatorKind kind = Stmt->getOperator();
	char const* opStr = getOperatorSpelling(kind);
	if(kind == OverloadedOperatorKind::OO_Call || kind == OverloadedOperatorKind::OO_Subscript)
	{
		auto iter = Stmt->arg_begin(), end = Stmt->arg_end();
		TraverseStmt(*iter);
		Spliter spliter(*this, ", ");
		out() << opStr[0];
		for(++iter; iter != end; ++iter)
		{
			if((*iter)->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(*iter);
			}
		}
		out() << opStr[1];
	}
	else if(kind == OverloadedOperatorKind::OO_Arrow)
		TraverseStmt(*Stmt->arg_begin());
	else if(kind == OverloadedOperatorKind::OO_Equal)
	{
		Expr* lo = *Stmt->arg_begin();
		Expr* ro = *(Stmt->arg_end() - 1);

		bool const lo_ptr = isPointer(lo->getType());
		bool const ro_ptr = isPointer(ro->getType());

		TypeOptions::Semantic const lo_sem = getSemantic(lo->getType());
		TypeOptions::Semantic const ro_sem = getSemantic(ro->getType());

		bool const dup = //both operands will be transformed to pointer
		  (ro_ptr == false && ro_sem != TypeOptions::Value) &&
		  (lo_ptr == false && lo_sem != TypeOptions::Value);

		if(dup)
		{
			// Always use dup, because
			//  - It is OK on hashmap
			//  - opAssign is not possible on classes
			//  - copy ctor is possible but can cause slicing
			TraverseStmt(lo);
			out() << " = ";
			TraverseStmt(ro);
			out() << ".dup()";
			isThisFunctionUsefull = true;
		}
		else
		{
			TraverseStmt(lo);
			out() << " = ";
			TraverseStmt(ro);
		}
	}
	else if(kind == OverloadedOperatorKind::OO_PlusPlus ||
	        kind == OverloadedOperatorKind::OO_MinusMinus)
	{
		if(numArgs == 2)
		{
			TraverseStmt(*Stmt->arg_begin());
			out() << opStr;
		}
		else
		{
			out() << opStr;
			TraverseStmt(*Stmt->arg_begin());
		}
	}
	else
	{
		if(numArgs == 2)
		{
			TraverseStmt(*Stmt->arg_begin());
			out() << " ";
		}
		out() << opStr;
		if(numArgs == 2)
			out() << " ";
		TraverseStmt(*(Stmt->arg_end() - 1));
	}
	return true;
}

bool DPrinter::TraverseExprWithCleanups(ExprWithCleanups* Stmt)
{
	if(passStmt(Stmt)) return true;
	TraverseStmt(Stmt->getSubExpr());
	return true;
}

void DPrinter::traverseCompoundStmtOrNot(Stmt* Stmt)  //Impl
{
	if(Stmt->getStmtClass() == Stmt::StmtClass::CompoundStmtClass)
	{
		out() << indentStr();
		TraverseStmt(Stmt);
	}
	else
	{
		++indent;
		out() << indentStr();
		if(isa<NullStmt>(Stmt))
			out() << "{}";
		TraverseStmt(Stmt);
		if(needSemiComma(Stmt))
			out() << ";";
		--indent;
	}
}

bool DPrinter::TraverseArraySubscriptExpr(ArraySubscriptExpr* Expr)
{
	if(passStmt(Expr)) return true;
	TraverseStmt(Expr->getLHS());
	out() << '[';
	TraverseStmt(Expr->getRHS());
	out() << ']';
	return true;
}

bool DPrinter::TraverseFloatingLiteral(FloatingLiteral* Expr)
{
	if(passStmt(Expr)) return true;
	const llvm::fltSemantics& sem = Expr->getSemantics();
	llvm::SmallString<1000> str;
	if(APFloat::semanticsSizeInBits(sem) < 64)
	{
		Expr->getValue().toString(str, std::numeric_limits<float>::digits10);
		out() << str.c_str() << 'f';
	}
	else if(APFloat::semanticsSizeInBits(sem) > 64)
	{
		Expr->getValue().toString(str, std::numeric_limits<long double>::digits10);
		out() << str.c_str() << 'l';
	}
	else
	{
		Expr->getValue().toString(str, std::numeric_limits<double>::digits10);
		out() << str.c_str();
	}
	return true;
}

bool DPrinter::TraverseForStmt(ForStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "for(";
	splitMultiLineDecl = false;
	TraverseStmt(Stmt->getInit());
	splitMultiLineDecl = true;
	out() << "; ";
	TraverseStmt(Stmt->getCond());
	out() << "; ";
	TraverseStmt(Stmt->getInc());
	out() << ")" << std::endl;
	traverseCompoundStmtOrNot(Stmt->getBody());
	return true;
}

bool DPrinter::TraverseWhileStmt(WhileStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "while(";
	TraverseStmt(Stmt->getCond());
	out() << ")" << std::endl;
	traverseCompoundStmtOrNot(Stmt->getBody());
	return true;
}

bool DPrinter::TraverseIfStmt(IfStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	out() << "if(";
	TraverseStmt(Stmt->getCond());
	out() << ")" << std::endl;
	traverseCompoundStmtOrNot(Stmt->getThen());
	if(Stmt->getElse())
	{
		out() << std::endl << indentStr() << "else ";
		if(Stmt->getElse()->getStmtClass() == Stmt::IfStmtClass)
			TraverseStmt(Stmt->getElse());
		else
		{
			out() << std::endl;
			traverseCompoundStmtOrNot(Stmt->getElse());
		}
	}
	return true;
}

bool DPrinter::TraverseCXXBindTemporaryExpr(CXXBindTemporaryExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	TraverseStmt(Stmt->getSubExpr());
	return true;
}

bool DPrinter::TraverseCXXThrowExpr(CXXThrowExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << "throw ";
	if(Stmt->getSubExpr() == nullptr)
		out() << catchedExceptNames.top();
	else
		TraverseStmt(Stmt->getSubExpr());
	return true;
}

bool DPrinter::TraverseMaterializeTemporaryExpr(MaterializeTemporaryExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	TraverseStmt(Stmt->GetTemporaryExpr());
	return true;
}

bool DPrinter::TraverseCXXFunctionalCastExpr(CXXFunctionalCastExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	QualType qt = Stmt->getTypeInfoAsWritten()->getType();
	if(getSemantic(qt) == TypeOptions::Reference)
	{
		out() << "new ";
		printType(qt);
		out() << '(';
		TraverseStmt(Stmt->getSubExpr());
		out() << ')';
	}
	else
	{
		out() << "cast(";
		printType(qt);
		out() << ")(";
		TraverseStmt(Stmt->getSubExpr());
		out() << ")";
	}
	return true;
}

bool DPrinter::TraverseParenType(ParenType* Type)
{
	if(passType(Type)) return false;
	// Parenthesis are useless (and illegal) on function types
	printType(Type->getInnerType());
	return true;
}

bool DPrinter::TraverseFunctionProtoType(FunctionProtoType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getReturnType());
	out() << " function(";
	Spliter spliter(*this, ", ");
	for(auto const& p : Type->getParamTypes())
	{
		spliter.split();
		printType(p);
	}
	if(Type->isVariadic())
	{
		spliter.split();
		out() << "...";
		addExternInclude("core.vararg", "...");
	}
	out() << ')';
	return true;
}

bool DPrinter::TraverseCXXTemporaryObjectExpr(CXXTemporaryObjectExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	printType(Stmt->getType());
	out() << '(';
	TraverseCXXConstructExpr(Stmt);
	out() << ')';
	return true;
}

bool DPrinter::TraverseNullStmt(NullStmt* Stmt)
{
	if(passStmt(Stmt)) return false;
	return true;
}

bool DPrinter::TraverseCharacterLiteral(CharacterLiteral* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << '\'';
	auto c = Stmt->getValue();
	switch(c)
	{
	case '\0': out() << "\\0"; break;
	case '\n': out() << "\\n"; break;
	case '\t': out() << "\\t"; break;
	case '\r': out() << "\\r"; break;
	default: out() << (char)c;
	}
	out() << '\'';
	return true;
}

bool DPrinter::TraverseStringLiteral(clang::StringLiteral* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << "\"";
	std::string literal;
	StringRef str = Stmt->getString();
	if(not str.empty())
	{
		if(Stmt->isUTF16() || Stmt->isWide())
		{
			const UTF16* source = reinterpret_cast<const UTF16*>(str.begin());
			const UTF16* sourceEnd = reinterpret_cast<const UTF16*>(str.end());
			literal.resize(size_t((sourceEnd - source) * UNI_MAX_UTF8_BYTES_PER_CODE_POINT + 1));
			auto* dest = reinterpret_cast<UTF8*>(&literal[0]);
			ConvertUTF16toUTF8(&source, source + str.size(), &dest, dest + literal.size(), ConversionFlags());
			literal.resize(size_t(reinterpret_cast<char*>(dest) - &literal[0]));
		}
		else if(Stmt->isUTF32())
		{
			const UTF32* source = reinterpret_cast<const UTF32*>(str.begin());
			const UTF32* sourceEnd = reinterpret_cast<const UTF32*>(str.end());
			literal.resize(size_t((sourceEnd - source) * UNI_MAX_UTF8_BYTES_PER_CODE_POINT + 1));
			auto* dest = reinterpret_cast<UTF8*>(&literal[0]);
			ConvertUTF32toUTF8(&source, source + str.size(), &dest, dest + literal.size(), ConversionFlags());
			literal.resize(size_t(reinterpret_cast<char*>(dest) - &literal[0]));
		}
		else
		{
			for (char _c : str)
			{
				unsigned char c = (unsigned char)_c;
				if (c < 128)
					literal.push_back(_c);
				else
				{
					static char buffer[20];
					std::sprintf(buffer, "\\x%x", c);
					literal += buffer;
				}
			}
		}
	}
	size_t pos = 0;
	while((pos = literal.find('\\', pos)) != std::string::npos)
	{
		char next = ' ';
		if (literal.size() > (pos + 1))
			next = literal[pos + 1];
		if (next != 'n' && next != 'x')
		{
			literal = literal.substr(0, pos) + "\\\\" + literal.substr(pos + 1);
			pos += 2;
		}
		else
			++pos;
	}
	pos = std::string::npos;
	while((pos = literal.find('\n')) != std::string::npos)
		literal = literal.substr(0, pos) + "\\n" + literal.substr(pos + 1);
	pos = 0;
	while((pos = literal.find('"', pos)) != std::string::npos)
	{
		if(pos > 0 && literal[pos - 1] == '\\')
			++pos;
		else
		{
			literal = literal.substr(0, pos) + "\\\"" + literal.substr(pos + 1);
			pos += 2;
		}
	}
	out() << literal;
	out() << "\"";
	return true;
}

bool DPrinter::TraverseCXXBoolLiteralExpr(CXXBoolLiteralExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << (Stmt->getValue() ? "true" : "false");
	return true;
}

bool DPrinter::TraverseUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr* Expr)
{
	if(passStmt(Expr)) return true;
	out() << '(';
	if(Expr->isArgumentType())
		printType(Expr->getArgumentType());
	else
		TraverseStmt(Expr->getArgumentExpr());
	UnaryExprOrTypeTrait const kind = Expr->getKind();
	out() << (
	        kind == UETT_AlignOf					? ").alignof"					:
	        kind == UETT_SizeOf						? ").sizeof"					:
	        kind == UETT_OpenMPRequiredSimdAlign	? ").OpenMPRequiredSimdAlign"	:
	        kind == UETT_VecStep					? ").VecStep"					:
	        ")");
	return true;
}

bool DPrinter::TraverseEmptyDecl(EmptyDecl* Decl)
{
	if(passDecl(Decl)) return true;
	return true;
}


bool DPrinter::TraverseLambdaExpr(LambdaExpr* Node)
{
	if(passStmt(Node)) return true;
	CXXMethodDecl* Method = Node->getCallOperator();

	// Has some auto type?
	bool hasAuto = false;
	if(Node->hasExplicitParameters())
	{
		for(ParmVarDecl* P : Method->parameters())
		{
			if(P->getType()->getTypeClass() == clang::Type::TypeClass::TemplateTypeParm)
			{
				hasAuto = true;
				break;
			}
			else if(auto* lvalueRef = dyn_cast<LValueReferenceType>(P->getType()))
			{
				if(lvalueRef->getPointeeType()->getTypeClass() == clang::Type::TypeClass::TemplateTypeParm)
				{
					hasAuto = true;
					break;
				}
			}
		}
	}

	if(hasAuto)
	{
		externIncludes["cpp_std"].insert("toFunctor");
		out() << "toFunctor!(";
	}

	const FunctionProtoType* Proto = Method->getType()->getAs<FunctionProtoType>();

	if(Node->hasExplicitResultType())
	{
		out() << "function ";
		printType(Proto->getReturnType());
	}

	if(Node->hasExplicitParameters())
	{
		out() << "(";
		inFuncParams = true;
		refAccepted = true;
		Spliter split(*this, ", ");
		for(ParmVarDecl* P : Method->parameters())
		{
			split.split();
			TraverseDecl(P);
		}
		if(Method->isVariadic())
		{
			split.split();
			out() << "...";
			addExternInclude("core.vararg", "...");
		}
		out() << ')';
		inFuncParams = false;
		refAccepted = false;
	}

	// Print the body.
	out() << "\n" << indentStr();
	CompoundStmt* Body = Node->getBody();
	TraverseStmt(Body);
	if(hasAuto)
		out() << ")()";
	return true;
}

void DPrinter::printCallExprArgument(CallExpr* Stmt)
{
	out() << "(";
	Spliter spliter(*this, ", ");
	for(Expr* arg : Stmt->arguments())
	{
		if(arg->getStmtClass() == Stmt::StmtClass::CXXDefaultArgExprClass)
			break;
		spliter.split();
		TraverseStmt(arg);
	}
	out() << ")";
}

bool DPrinter::TraverseCallExpr(CallExpr* Stmt)
{
	auto matchesName = [&](std::string const calleeName)
	{
		for(auto const& name_printer : receiver.globalFuncPrinters)
		{
			llvm::Regex RE(name_printer.first);
			if(RE.match("::" + calleeName))
			{
				name_printer.second(*this, Stmt);
				return true;
			}
		}
		return false;
	};
	if(Decl* calleeDecl = Stmt->getCalleeDecl())
	{
		if(auto* func = dyn_cast<FunctionDecl>(calleeDecl))
		{
			if(matchesName(func->getQualifiedNameAsString()))
				return true;
		}
	}
	Expr* callee = Stmt->getCallee();
	if(auto* lockup = dyn_cast<UnresolvedLookupExpr>(callee))
	{
		std::string name;
		llvm::raw_string_ostream ss(name);
		lockup->printPretty(ss, nullptr, printingPolicy);
		if(matchesName(ss.str()))
			return true;
	}

	if(passStmt(Stmt)) return true;
	dontTakePtr.insert(callee);
	TraverseStmt(callee);
	dontTakePtr.erase(callee);
	// Are parentezis on zero argument needed?
	//if (Stmt->getNumArgs() == 0)
	//	return true;
	printCallExprArgument(Stmt);
	return true;
}

bool DPrinter::TraverseImplicitCastExpr(ImplicitCastExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	if(Stmt->getCastKind() == CK_FunctionToPointerDecay && dontTakePtr.count(Stmt) == 0)
		out() << "&";
	if(Stmt->getCastKind() == CK_ConstructorConversion)
	{
		QualType const type = Stmt->getType();
		if(getSemantic(type) == TypeOptions::Reference)
			out() << "new ";
		printType(type);
		out() << '(';
	}
	TraverseStmt(Stmt->getSubExpr());
	if(Stmt->getCastKind() == CK_ConstructorConversion)
		out() << ')';
	return true;
}

bool DPrinter::TraverseCXXThisExpr(CXXThisExpr* expr)
{
	if(passStmt(expr)) return true;
	QualType pointee = expr->getType()->getPointeeType();
	if(getSemantic(pointee) == TypeOptions::Value)
		out() << "(&this)[0..1]";
	else
		out() << "this";
	return true;
}

bool DPrinter::isStdArray(QualType const& type)
{
	QualType const rawType = type.isCanonical() ?
	                         type :
	                         type.getCanonicalType();
	std::string const name = rawType.getAsString();
	static std::string const arrayNames[] =
	{
		"class boost::array<",
		"class std::array<",
		"struct boost::array<",
		"struct std::array<"
	};
	return std::any_of(std::begin(arrayNames), std::end(arrayNames), [&](auto && arrayName)
	{
		return name.find(arrayName) == 0;
	});
}

bool DPrinter::isStdUnorderedMap(QualType const& type)
{
	QualType const rawType = type.isCanonical() ?
	                         type :
	                         type.getCanonicalType();
	std::string const name = rawType.getAsString();
	static std::string const arrayNames[] =
	{
		"class std::unordered_map<",
		"class boost::unordered_map<",
		"struct std::unordered_map<",
		"struct boost::unordered_map<",
	};
	return std::any_of(std::begin(arrayNames), std::end(arrayNames), [&](auto && arrayName)
	{
		return name.find(arrayName) == 0;
	});
}

bool DPrinter::TraverseCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr* expr)
{
	if(passStmt(expr)) return true;
	return traverseMemberExprImpl(expr);
}

bool DPrinter::TraverseMemberExpr(MemberExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	return traverseMemberExprImpl(Stmt);
}

template<typename ME>
bool DPrinter::traverseMemberExprImpl(ME* Stmt)
{
	DeclarationName const declName = Stmt->getMemberNameInfo().getName();
	auto const kind = declName.getNameKind();
	std::string const memberName = Stmt->getMemberNameInfo().getName().getAsString();
	Expr* base = Stmt->isImplicitAccess() ? nullptr : Stmt->getBase();
	bool const isThis = not(base && base->getStmtClass() != Stmt::StmtClass::CXXThisExprClass);
	if(not isThis)
		TraverseStmt(base);
	if(kind == DeclarationName::NameKind::CXXConversionFunctionName)
	{
		if(memberName.empty() == false && not isThis)
			out() << '.';
		out() << "opCast!(";
		printType(declName.getCXXNameType());
		out() << ')';
	}
	else if(kind == DeclarationName::NameKind::CXXOperatorName)
	{
		out() << " " << memberName.substr(8) << " "; //8 is size of "operator"
	}
	else
	{
		if (memberName.empty() == false && not isThis)
		{	
			if (Stmt->isArrow() &&
				base->getStmtClass() != clang::Stmt::CXXOperatorCallExprClass &&
				getSemantic(base->getType()) == TypeOptions::Semantic::Value)
			{
				out() << ".front.";
				addExternInclude("std.range", "front");
			}
			else
				out() << '.';
		}
		out() << memberName;
	}
	auto TAL = Stmt->getTemplateArgs();
	auto const tmpArgCount = Stmt->getNumTemplateArgs();
	Spliter spliter(*this, ", ");
	if(tmpArgCount != 0)
	{
		pushStream();
		for(size_t I = 0; I < tmpArgCount; ++I)
		{
			spliter.split();
			printTemplateArgument(TAL[I].getArgument());
		}
		printTmpArgList(popStream());
	}

	return true;
}

//! @brief Is decl or parent of decl present in classes
//! @return The printer method found in classes
MatchContainer::ClassPrinter::const_iterator::value_type::second_type
isAnyOfThoseTypes(CXXRecordDecl* decl, MatchContainer::ClassPrinter const& classes)
{
	std::string declName = decl->getQualifiedNameAsString();
	size_t pos = declName.find('<');
	if(pos != std::string::npos)
		declName = declName.substr(0, pos);

	auto classIter = classes.find(declName);
	if(classIter != classes.end())
		return classIter->second;
	for(CXXBaseSpecifier const& baseSpec : decl->bases())
	{
		CXXRecordDecl* base = baseSpec.getType()->getAsCXXRecordDecl();
		assert(base);
		if(auto func = isAnyOfThoseTypes(base, classes))
			return func;
	}
	for(CXXBaseSpecifier const& baseSpec : decl->vbases())
	{
		CXXRecordDecl* base = baseSpec.getType()->getAsCXXRecordDecl();
		assert(base);
		if(auto func = isAnyOfThoseTypes(base, classes))
			return func;
	}
	return nullptr;
};

bool DPrinter::TraverseCXXMemberCallExpr(CXXMemberCallExpr* expr)
{
	if(passStmt(expr)) return true;
	if(auto* meth = dyn_cast<CXXMethodDecl>(expr->getCalleeDecl()))
	{
		std::string methName = meth->getNameAsString();

		CXXRecordDecl* thisType = expr->getRecordDecl();
		auto methIter = receiver.methodPrinters.find(methName);
		if(methIter != receiver.methodPrinters.end())
		{
			if(auto func = isAnyOfThoseTypes(thisType, methIter->second))
			{
				func(*this, expr);
				return true;
			}
		}
	}
	if(auto* dtor = dyn_cast<CXXDestructorDecl>(expr->getCalleeDecl()))
	{
		out() << "object.destroy(";
		if(auto* memExpr = dyn_cast<MemberExpr>(expr->getCallee()))
		{
			Expr* base = memExpr->isImplicitAccess() ? nullptr : memExpr->getBase();
			TraverseStmt(base);
		}
		out() << ")";
	}
	else
	{
		TraverseStmt(expr->getCallee());
		printCallExprArgument(expr);
	}
	return true;
}

bool DPrinter::TraverseCXXStaticCastExpr(CXXStaticCastExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << "cast(";
	printType(Stmt->getTypeInfoAsWritten()->getType());
	out() << ')';
	TraverseStmt(Stmt->getSubExpr());
	return true;
}

bool DPrinter::TraverseCStyleCastExpr(CStyleCastExpr* Stmt)
{
	if(passStmt(Stmt)) return true;
	out() << "cast(";
	printType(Stmt->getTypeInfoAsWritten()->getType());
	out() << ')';
	TraverseStmt(Stmt->getSubExpr());
	return true;
}

bool DPrinter::TraverseConditionalOperator(ConditionalOperator* op)
{
	if(passStmt(op)) return true;
	TraverseStmt(op->getCond());
	out() << "? ";
	TraverseStmt(op->getTrueExpr());
	out() << ": ";
	TraverseStmt(op->getFalseExpr());
	return true;
}

bool DPrinter::TraverseCompoundAssignOperator(CompoundAssignOperator* op)
{
	if(passStmt(op)) return true;
	DPrinter::TraverseBinaryOperator(op);
	return true;
}

bool DPrinter::TraverseBinAddAssign(CompoundAssignOperator* expr)
{
	if(passStmt(expr)) return true;
	if(isPointer(expr->getLHS()->getType()))
	{
		TraverseStmt(expr->getLHS());
		out() << ".popFrontN(";
		TraverseStmt(expr->getRHS());
		out() << ')';
		externIncludes["std.range.primitives"].insert("popFrontN");
		return true;
	}
	else
		return TraverseCompoundAssignOperator(expr);
}


#define OPERATOR(NAME)                                        \
	bool DPrinter::TraverseBin##NAME##Assign(CompoundAssignOperator *S) \
	{if (passStmt(S)) return true; return TraverseCompoundAssignOperator(S);}
OPERATOR(Mul) OPERATOR(Div) OPERATOR(Rem) OPERATOR(Sub)
OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or) OPERATOR(Xor)
#undef OPERATOR


bool DPrinter::TraverseSubstNonTypeTemplateParmExpr(SubstNonTypeTemplateParmExpr* Expr)
{
	if(passStmt(Expr)) return true;
	TraverseStmt(Expr->getReplacement());
	return true;
}

bool DPrinter::TraverseBinaryOperator(BinaryOperator* Stmt)
{
	if(passStmt(Stmt)) return true;
	Expr* lhs = Stmt->getLHS();
	Expr* rhs = Stmt->getRHS();
	if(isPointer(lhs->getType()) and isPointer(rhs->getType()))
	{
		TraverseStmt(Stmt->getLHS());
		switch(Stmt->getOpcode())
		{
		case BinaryOperatorKind::BO_EQ: out() << " is "; break;
		case BinaryOperatorKind::BO_NE: out() << " !is "; break;
		default: out() << " " << Stmt->getOpcodeStr().str() << " ";
		}
		TraverseStmt(Stmt->getRHS());
	}
	else
	{
		TraverseStmt(lhs);
		out() << " " << Stmt->getOpcodeStr().str() << " ";
		TraverseStmt(rhs);
	}
	return true;
}

bool DPrinter::TraverseBinAdd(BinaryOperator* expr)
{
	if(passStmt(expr)) return true;
	if(isPointer(expr->getLHS()->getType()))
	{
		TraverseStmt(expr->getLHS());
		out() << '[';
		TraverseStmt(expr->getRHS());
		out() << "..$]";
		return true;
	}
	else
		return TraverseBinaryOperator(expr);
}

#define OPERATOR(NAME) \
	bool DPrinter::TraverseBin##NAME(BinaryOperator* Stmt) \
	{if (passStmt(Stmt)) return true; return TraverseBinaryOperator(Stmt);}
OPERATOR(PtrMemD) OPERATOR(PtrMemI) OPERATOR(Mul) OPERATOR(Div)
OPERATOR(Rem) OPERATOR(Sub) OPERATOR(Shl) OPERATOR(Shr)
OPERATOR(LT) OPERATOR(GT) OPERATOR(LE) OPERATOR(GE) OPERATOR(EQ)
OPERATOR(NE) OPERATOR(And) OPERATOR(Xor) OPERATOR(Or) OPERATOR(LAnd)
OPERATOR(LOr) OPERATOR(Assign) OPERATOR(Comma)
#undef OPERATOR

bool DPrinter::TraverseUnaryOperator(UnaryOperator* Stmt)
{
	if(passStmt(Stmt)) return true;
	if(Stmt->isIncrementOp())
	{
		if(isPointer(Stmt->getSubExpr()->getType()))
		{
			TraverseStmt(Stmt->getSubExpr());
			out() << ".popFront";
			externIncludes["std.range.primitives"].insert("popFront");
			return true;
		}
	}

	if(Stmt->isPostfix())
	{
		TraverseStmt(Stmt->getSubExpr());
		out() << Stmt->getOpcodeStr(Stmt->getOpcode()).str();
	}
	else
	{
		std::string preOp = Stmt->getOpcodeStr(Stmt->getOpcode()).str();
		std::string postOp = "";
		if(Stmt->getOpcode() == UnaryOperatorKind::UO_AddrOf)
		{
			preOp = "(&";
			postOp = ")[0..1]";
		}
		else if(Stmt->getOpcode() == UnaryOperatorKind::UO_Deref)
		{
			if(auto* t = dyn_cast<CXXThisExpr>(Stmt->getSubExpr()))
			{
				// (*this) in C++ mean (this) in D
				out() << "this";
				return true;
			}

			preOp.clear();
			postOp = "[0]";
		}

		// Avoid to deref struct this
		Expr* expr = static_cast<Expr*>(*Stmt->child_begin());
		bool showOp = true;

		QualType exprType = expr->getType();
		TypeOptions::Semantic operSem =
		  exprType->hasPointerRepresentation() ?
		  getSemantic(exprType->getPointeeType()) :
		  getSemantic(exprType);

		if(operSem != TypeOptions::Value)
		{
			if(Stmt->getOpcode() == UnaryOperatorKind::UO_AddrOf
			   || Stmt->getOpcode() == UnaryOperatorKind::UO_Deref)
				showOp = false;
		}
		if(showOp)
			out() << preOp;
		for(auto c : Stmt->children())
			TraverseStmt(c);
		if(showOp)
			out() << postOp;
	}
	return true;
}
#define OPERATOR(NAME) \
	bool DPrinter::TraverseUnary##NAME(UnaryOperator* Stmt) \
	{if (passStmt(Stmt)) return true; return TraverseUnaryOperator(Stmt);}
OPERATOR(PostInc) OPERATOR(PostDec) OPERATOR(PreInc) OPERATOR(PreDec)
OPERATOR(AddrOf) OPERATOR(Deref) OPERATOR(Plus) OPERATOR(Minus)
OPERATOR(Not) OPERATOR(LNot) OPERATOR(Real) OPERATOR(Imag)
OPERATOR(Extension) OPERATOR(Coawait)
#undef OPERATOR

template<typename TDeclRefExpr>
void DPrinter::traverseDeclRefExprImpl(TDeclRefExpr* Expr)
{
	size_t const argNum = Expr->getNumTemplateArgs();
	if(argNum != 0)
	{
		TemplateArgumentLoc const* tmpArgs = Expr->getTemplateArgs();
		Spliter split(*this, ", ");
		pushStream();
		for(size_t i = 0; i < argNum; ++i)
		{
			split.split();
			printTemplateArgument(tmpArgs[i].getArgument());
		}
		printTmpArgList(popStream());
	}
}

bool DPrinter::TraverseDeclRefExpr(DeclRefExpr* Expr)
{
	if(passStmt(Expr)) return true;
	QualType nnsQualType;
	NestedNameSpecifier* nns = Expr->getQualifier();
	if(nns != nullptr)
	{
		if(nns->getKind() == NestedNameSpecifier::SpecifierKind::TypeSpec)
			nnsQualType = nns->getAsType()->getCanonicalTypeUnqualified();
	}
	auto decl = Expr->getDecl();
	bool nestedNamePrined = false;
	if(decl->getKind() == Decl::Kind::EnumConstant)
	{
		nestedNamePrined = true;
		if(nnsQualType != decl->getType().getUnqualifiedType())
		{
			printType(decl->getType().getUnqualifiedType());
			out() << '.';
		}
		else if(nns)
			TraverseNestedNameSpecifier(nns);
	}
	else if(nns)
	{
		nestedNamePrined = true;
		TraverseNestedNameSpecifier(nns);
	}
	else
	{	// If it refer the param argc, print argv.length
		ValueDecl* valdecl = Expr->getDecl();
		ParmVarDecl* pVarDecl = llvm::dyn_cast<ParmVarDecl>(valdecl);
		if (pVarDecl && pVarDecl->getNameAsString() == "argc")
		{
			out() << "(cast(int)argv.length)";
			return true;
		}
	}

	std::string name = getName(Expr->getNameInfo().getName());
	if(nestedNamePrined == false)
		if(char const* filename = CPP2DTools::getFile(Context->getSourceManager(), Expr->getDecl()))
			includeFile(filename, name);
	out() << mangleName(name);
	traverseDeclRefExprImpl(Expr);
	return true;
}

bool DPrinter::TraverseDependentScopeDeclRefExpr(DependentScopeDeclRefExpr* Expr)
{
	if(passStmt(Expr)) return true;
	if(NestedNameSpecifier* nns = Expr->getQualifier())
		TraverseNestedNameSpecifier(nns);
	out() << mangleName(getName(Expr->getNameInfo().getName()));
	traverseDeclRefExprImpl(Expr);
	return true;

}

bool DPrinter::TraverseUnresolvedLookupExpr(UnresolvedLookupExpr*  Expr)
{
	if(passStmt(Expr)) return true;
	if(NestedNameSpecifier* nns = Expr->getQualifier())
		TraverseNestedNameSpecifier(nns);
	out() << mangleName(getName(Expr->getNameInfo().getName()));
	traverseDeclRefExprImpl(Expr);
	return true;
}

bool DPrinter::TraverseRecordType(RecordType* Type)
{
	if(passType(Type)) return false;
	if(customTypePrinter(Type->getDecl()))
		return true;

	out() << printDeclName(Type->getDecl());
	RecordDecl* decl = Type->getDecl();
	switch(decl->getKind())
	{
	case Decl::Kind::Record:	break;
	case Decl::Kind::CXXRecord:	break;
	case Decl::Kind::ClassTemplateSpecialization:
	{
		// Print template arguments in template type of template specialization
		auto* tmpSpec = llvm::dyn_cast<ClassTemplateSpecializationDecl>(decl);
		TemplateArgumentList const& tmpArgsSpec = tmpSpec->getTemplateInstantiationArgs();
		pushStream();
		Spliter spliter2(*this, ", ");
		for(unsigned int i = 0, size = tmpArgsSpec.size(); i != size; ++i)
		{
			spliter2.split();
			TemplateArgument const& tmpArg = tmpArgsSpec.get(i);
			printTemplateArgument(tmpArg);
		}
		printTmpArgList(popStream());
		break;
	}
	default: assert(false && "Unconsustent RecordDecl kind");
	}
	return true;
}

bool DPrinter::TraverseConstantArrayType(ConstantArrayType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getElementType());
	out() << '[' << Type->getSize().toString(10, false) << ']';
	return true;
}

bool DPrinter::TraverseIncompleteArrayType(IncompleteArrayType* Type)
{
	if(passType(Type)) return false;
	printType(Type->getElementType());
	out() << "[]";
	return true;
}

bool DPrinter::TraverseDependentSizedArrayType(DependentSizedArrayType* type)
{
	if(passType(type)) return false;
	printType(type->getElementType());
	out() << '[';
	TraverseStmt(type->getSizeExpr());
	out() << ']';
	return true;
}

bool DPrinter::TraverseInitListExpr(InitListExpr* expr)
{
	if(passStmt(expr)) return true;
	Expr* expr2 = expr->IgnoreImplicit();
	if(expr2 != expr)
		return TraverseStmt(expr2);

	bool isExplicitBracket = true;
	if(expr->getNumInits() == 1)
		isExplicitBracket = not isa<InitListExpr>(expr->getInit(0));

	bool const isArray =
	  (expr->ClassifyLValue(*Context) == Expr::LV_ArrayTemporary);
	if(isExplicitBracket)
		out() << (isArray ? '[' : '{') << " " << std::endl;
	++indent;
	size_t argIndex = 0;
	for(Expr* c : expr->inits())
	{
		++argIndex;
		pushStream();
		TraverseStmt(c);
		std::string const valInit = popStream();
		if(valInit.empty() == false)
		{
			out() << indentStr() << valInit;
			if(isExplicitBracket)
				out() << ',' << std::endl;
		}
		output_enabled = (isInMacro == 0);
	}
	--indent;
	if(isExplicitBracket)
		out() << indentStr() << (isArray ? ']' : '}');
	return true;
}

bool DPrinter::TraverseParenExpr(ParenExpr* expr)
{
	if(passStmt(expr)) return true;
	if(auto* binOp = dyn_cast<BinaryOperator>(expr->getSubExpr()))
	{
		Expr* lhs = binOp->getLHS();
		Expr* rhs = binOp->getRHS();
		clang::StringLiteral const* strLit = dyn_cast<clang::StringLiteral>(lhs);
		if(strLit && (binOp->getOpcode() == BinaryOperatorKind::BO_Comma))
		{
			StringRef const str = strLit->getString();
			if(str == "CPP2D_MACRO_EXPR")
			{
				auto get_binop = [](Expr * paren)
				{
					return dyn_cast<BinaryOperator>(dyn_cast<ParenExpr>(paren)->getSubExpr());
				};
				BinaryOperator* macro_and_cpp = get_binop(rhs);
				BinaryOperator* macro_name_and_args = get_binop(macro_and_cpp->getLHS());
				auto* macro_name = dyn_cast<clang::StringLiteral>(macro_name_and_args->getLHS());
				auto* macro_args = dyn_cast<CallExpr>(macro_name_and_args->getRHS());
				std::string macroName = macro_name->getString().str();
				if(macroName == "assert")
				{
					out() << "assert(";
					TraverseStmt(*macro_args->arg_begin());
					out() << ")";
				}
				else
				{
					out() << "(mixin(" << macroName << "!(";
					printMacroArgs(macro_args);
					out() << ")))";
				}
				pushStream();
				TraverseStmt(macro_and_cpp->getRHS()); //Add the required import
				popStream();
				return true;
			}
		}
	}
	out() << '(';
	TraverseStmt(expr->getSubExpr());
	out() << ')';
	return true;
}

bool DPrinter::TraverseImplicitValueInitExpr(ImplicitValueInitExpr* expr)
{
	if(passStmt(expr)) return true;
	return true;
}

bool DPrinter::TraverseParenListExpr(clang::ParenListExpr* expr)
{
	if(passStmt(expr)) return true;
	Spliter split(*this, ", ");
	for(Expr* arg : expr->exprs())
	{
		split.split();
		TraverseStmt(arg);
	}
	return true;
}

bool DPrinter::TraverseCXXScalarValueInitExpr(CXXScalarValueInitExpr* expr)
{
	QualType type;
	if(TypeSourceInfo* TSInfo = expr->getTypeSourceInfo())
		type = TSInfo->getType();
	else
		type = expr->getType();
	if(type->isPointerType())
		out() << "null";
	else
	{
		printType(type);
		out() << "()";
	}

	return true;
}

bool DPrinter::TraverseUnresolvedMemberExpr(UnresolvedMemberExpr* expr)
{
	if(!expr->isImplicitAccess())
	{
		TraverseStmt(expr->getBase());
		out() << '.';
	}
	if(NestedNameSpecifier* Qualifier = expr->getQualifier())
		TraverseNestedNameSpecifier(Qualifier);
	out() << expr->getMemberNameInfo().getAsString();
	traverseDeclRefExprImpl(expr);
	return true;
}

void DPrinter::traverseVarDeclImpl(VarDecl* Decl)
{
	std::string const varName = getName(Decl->getDeclName());
	if(varName.find("CPP2D_MACRO_STMT") == 0)
	{
		printStmtMacro(varName, Decl->getInit());
		return;
	}

	if(passDecl(Decl)) return;

	if(Decl->getCanonicalDecl() != Decl)
		return;

	if (Decl->isOutOfLine())
		return;
	else if (Decl->isOutOfLine())
		Decl = Decl->getDefinition();
	QualType varType = Decl->getType();
	bool const isRef = [&]()
	{
		if(auto* refType = varType->getAs<LValueReferenceType>())
		{
			if(not refAccepted && getSemantic(refType->getPointeeType()) == TypeOptions::Value)
				return true;  //Have to call "makeRef" to handle to storage of a ref
		}
		return false;
	}();

	if(doPrintType)
	{
		if(Decl->isStaticDataMember() || Decl->isStaticLocal())
			out() << "static ";
		if(isRef)
			out() << "auto";
		else
			printType(varType);
		out() << " ";
	}
	out() << mangleName(varName);
	bool const in_foreach_decl = inForRangeInit;
	VarDecl* definition = Decl->hasInit() ? Decl : Decl->getDefinition();
	if(definition && definition->hasInit() && !in_foreach_decl)
	{
		Expr* init = definition->getInit();
		if(definition->isDirectInit())
		{
			if(auto* constr = dynCastAcrossCleanup<CXXConstructExpr>(init))
			{
				if(getSemantic(varType) == TypeOptions::Value)
				{
					if(constr->getNumArgs() != 0)
					{
						out() << " = ";
						if(isRef)
							out() << "makeRef(";
						printCXXConstructExprParams(constr);
						if(isRef)
							out() << ")";
					}
				}
				else if(getSemantic(varType) == TypeOptions::AssocArray)
				{
					if(constr->getNumArgs() != 0 && not isa<CXXDefaultArgExpr>(*constr->arg_begin()))
					{
						out() << " = ";
						printCXXConstructExprParams(constr);
					}
				}
				else
				{
					out() << " = new ";
					printCXXConstructExprParams(constr);
				}
			}
			else
			{
				out() << " = ";
				TraverseStmt(init);
			}
		}
		else
		{
			out() << " = ";
			if(isRef)
				out() << "makeRef(";
			TraverseStmt(init);
			if(isRef)
				out() << ")";
		}
	}
}

bool DPrinter::TraverseVarDecl(VarDecl* Decl)
{
	if(passDecl(Decl)) return true;
	traverseVarDeclImpl(Decl);
	return true;
}

bool DPrinter::TraverseIndirectFieldDecl(IndirectFieldDecl*)
{
	return true;
}

bool DPrinter::VisitDecl(Decl* Decl)
{
	out() << indentStr() << "/*" << Decl->getDeclKindName() << " Decl*/";
	return true;
}

bool DPrinter::VisitStmt(Stmt* Stmt)
{
	out() << indentStr() << "/*" << Stmt->getStmtClassName() << " Stmt*/";
	return true;
}

bool DPrinter::VisitType(clang::Type* Type)
{
	out() << indentStr() << "/*" << Type->getTypeClassName() << " Type*/";
	return true;
}

void DPrinter::addExternInclude(std::string const& include, std::string const& typeName)
{
	externIncludes[include].insert(typeName);
}

std::ostream& DPrinter::stream()
{
	return out();
}

std::map<std::string, std::set<std::string> > const& DPrinter::getExternIncludes() const
{
	return externIncludes;
}


std::string DPrinter::getDCode() const
{
	return out().str();
}

bool DPrinter::shouldVisitImplicitCode() const
{
	return true;
}
