#include "VisitorToD.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <ciso646>

#pragma warning(push, 0)
#pragma warning(disable, 4702)
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroArgs.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/Path.h>
#include <clang/AST/Comment.h>
//#include "llvm/Support/ConvertUTF.h"
#pragma warning(pop)

#include "MatchContainer.h"
#include "Matcher.h"
#include "Find_Includes.h"

//using namespace clang::tooling;
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
	{ "std::set", "std.container.rbtree.RedBlackTree" },
	{ "std::logic_error", "object.error" },
	{ "std::runtime_error", "object.Exception" },
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
	{ "SafeInt", "std.experimental.safeint.SafeInt" },
	{ "RedBlackTree", "std.container.rbtree" },
	{ "std::map", "cpp_std.map" },
	{ "std::string", "string" },
	{ "std::ostream", "std.stdio.File" },
};


static const std::set<Decl::Kind> noSemiCommaDeclKind =
{
	Decl::Kind::CXXRecord,
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
	//Stmt::StmtClass::DeclStmtClass,
};

bool needSemiComma(Stmt* stmt)
{
	auto const kind = stmt->getStmtClass();
	if(kind == Stmt::StmtClass::DeclStmtClass)
	{
		auto declStmt = static_cast<DeclStmt*>(stmt);
		if(declStmt->isSingleDecl() == false)
			return false;
		else
			return noSemiCommaDeclKind.count(declStmt->getSingleDecl()->getKind()) == 0;
	}
	else
		return noSemiCommaStmtKind.count(kind) == 0;
}

bool needSemiComma(Decl* decl)
{
	if(decl->isImplicit())
		return false;
	auto const kind = decl->getKind();
	if(kind == Decl::Kind::CXXRecord)
	{
		auto record = static_cast<CXXRecordDecl*>(decl);
		return !record->isCompleteDefinition();
	}
	else
		return noSemiCommaDeclKind.count(kind) == 0;
}

struct Spliter
{
	std::string const str;
	bool first = true;

	Spliter(std::string const& s) : str(s) {}

	void split()
	{
		if(first)
			first = false;
		else
			out() << str;
	}
};

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
	"private"  // ...the special value "none" which means different things in different contexts. (from the clang doxy)
};

std::set<std::string> includes_in_file;

class VisitorToD
	: public RecursiveASTVisitor<VisitorToD>
{
	typedef RecursiveASTVisitor<VisitorToD> Base;

	void include_file(std::string const& decl_inc)
	{
		for(std::string include : includes_in_file)
		{
			auto const pos = decl_inc.find(include);
			if(pos != std::string::npos &&
			   pos == (decl_inc.size() - include.size()) &&
			   (pos == 0 || decl_inc[pos - 1] == '/' || decl_inc[pos - 1] == '\\'))
			{
				if(include.find(".h") == include.size() - 2)
					include = include.substr(0, include.size() - 2);
				if(include.find(".hpp") == include.size() - 4)
					include = include.substr(0, include.size() - 4);
				std::transform(std::begin(include), std::end(include),
				               std::begin(include),
				               tolower);
				std::replace(std::begin(include), std::end(include), '/', '.');
				std::replace(std::begin(include), std::end(include), '\\', '.');
				extern_includes.insert(include);
				break;
			}
		}
	}

	std::string mangleType(NamedDecl const* decl)
	{
		NamedDecl const* can_decl = nullptr;
		std::string const& name = decl->getNameAsString();
		std::string qual_name = name;
		if(Decl const* can_decl_notype = decl->getCanonicalDecl())
		{
			auto const kind = can_decl_notype->getKind();
			if(Decl::Kind::firstNamed <= kind && kind <= Decl::Kind::lastNamed)
			{
				can_decl = static_cast<NamedDecl const*>(can_decl_notype);
				qual_name = can_decl->getQualifiedNameAsString();
			}
		}

		auto qual_type_to_d = type2type.find(qual_name);
		if(qual_type_to_d != type2type.end())
		{
			//There is a convertion to D
			auto const& d_qual_type = qual_type_to_d->second;
			auto const dot_pos = d_qual_type.find_last_of('.');
			auto const module = dot_pos == std::string::npos ?
			                    std::string() :
			                    d_qual_type.substr(0, dot_pos);
			if(not module.empty())  //Need an import
				extern_includes.insert(module);
			return d_qual_type.substr(dot_pos + 1);
		}
		else
		{
			include_file(getFile(can_decl ? can_decl : decl));
			return name;
		}
	}

	std::string mangleVar(DeclRefExpr* expr)
	{
		std::string name = expr->getNameInfo().getName().getAsString();
		if(name == "printf")
		{
			extern_includes.insert("std.stdio");
			return "writef";
		}
		else
		{
			if(char const* filename = getFile(expr->getDecl()))
				include_file(filename);
			return mangleName(name);
		}
	}

	std::string replace(std::string str, std::string const& in, std::string const& out)
	{
		size_t pos = 0;
		std::string::iterator iter;
		while((iter = std::find(std::begin(str) + pos, std::end(str), '\r')) != std::end(str))
		{
			pos = iter - std::begin(str);
			if((pos + 1) < str.size() && str[pos + 1] == '\n')
			{
				str = str.substr(0, pos) + out + str.substr(pos + in.size());
				++pos;
			}
			else
				++pos;
		}
		return str;
	}

	void printCommentBefore(Decl* t)
	{
		SourceManager& sm = Context->getSourceManager();
		std::stringstream line_return;
		line_return << std::endl;
		const RawComment* rc = Context->getRawCommentForDeclNoCache(t);
		if(rc && not rc->isTrailingComment())
		{
			using namespace std;
			out() << std::endl << indent_str();
			string const comment = rc->getRawText(sm).str();
			out() << replace(comment, "\r\n", line_return.str()) << std::endl << indent_str();
		}
		else
			out() << std::endl << indent_str();
	}

	void printCommentAfter(Decl* t)
	{
		SourceManager& sm = Context->getSourceManager();
		const RawComment* rc = Context->getRawCommentForDeclNoCache(t);
		if(rc && rc->isTrailingComment())
			out() << '\t' << rc->getRawText(sm).str();
	}

	// trim from both ends
	static inline std::string trim(std::string const& s)
	{
		auto const pos1 = s.find_first_not_of("\r\n\t ");
		auto const pos2 = s.find_last_not_of("\r\n\t ");
		return pos1 == std::string::npos ?
		       std::string() :
		       s.substr(pos1, (pos2 - pos1) + 1);
	}

	std::vector<std::string> split(std::string const& instr)
	{
		std::vector<std::string> result;
		auto prevIter = std::begin(instr);
		auto iter = std::begin(instr);
		do
		{
			iter = std::find(prevIter, std::end(instr), '\n');
			result.push_back(trim(std::string(prevIter, iter)));
			if(iter != std::end(instr))
				prevIter = iter + 1;
		}
		while(iter != std::end(instr));
		return result;
	}

	void printStmtComment(SourceLocation& locStart,
	                      SourceLocation const& locEnd,
	                      SourceLocation const& nextStart = SourceLocation())
	{
		if(locStart.isInvalid() || locEnd.isInvalid() || locStart.isMacroID() || locEnd.isMacroID())
		{
			locStart = nextStart;
			out() << std::endl;
			return;
		}
		auto& sm = Context->getSourceManager();
		std::string comment =
		  Lexer::getSourceText(CharSourceRange(SourceRange(locStart, locEnd), true),
		                       sm,
		                       LangOptions()
		                      ).str();
		std::vector<std::string> comments = split(comment);
		//if (comments.back() == std::string())
		comments.pop_back();
		Spliter split(indent_str());
		if(comments.empty())
			out() << std::endl;
		if(not comments.empty())
		{
			auto& firstComment = comments.front();
			auto commentPos1 = firstComment.find("//");
			auto commentPos2 = firstComment.find("/*");
			size_t trimPos = 0;
			if(commentPos1 != std::string::npos)
			{
				if(commentPos2 != std::string::npos)
					trimPos = std::min(commentPos1, commentPos2);
				else
					trimPos = commentPos1;
			}
			else if(commentPos2 != std::string::npos)
				trimPos = commentPos2;
			else
				firstComment = "";

			if(not firstComment.empty())
				firstComment = firstComment.substr(trimPos);

			out() << " ";
			size_t index = 0;
			for(auto const& c : comments)
			{
				split.split();
				out() << c;
				out() << std::endl;
				++index;
			}
		}
		locStart = nextStart;
	}

	void PrintMacroArgs(CallExpr* macro_args)
	{
		Spliter split(", ");
		for(Expr* arg : macro_args->arguments())
		{
			split.split();
			out() << "q{";
			if(auto* callExpr = dyn_cast<CallExpr>(arg))
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
							auto* str = dyn_cast<StringLiteral>(impCast2->getSubExpr());
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
			out() << "}";
		}
	}

	void PrintStmtMacro(std::string const& varName, Expr* init)
	{
		if(varName.find("CPP2D_MACRO_STMT_END") == 0)
			--isInMacro;
		else if(varName.find("CPP2D_MACRO_STMT") == 0)
		{
			auto get_binop = [](Expr * paren)
			{
				return dyn_cast<BinaryOperator>(dyn_cast<ParenExpr>(paren)->getSubExpr());
			};
			BinaryOperator* name_and_args = get_binop(init); // Decl->getInClassInitializer());
			auto* macro_name = dyn_cast<StringLiteral>(name_and_args->getLHS());
			auto* macro_args = dyn_cast<CallExpr>(name_and_args->getRHS());
			out() << "mixin(" << macro_name->getString().str() << "!(";
			PrintMacroArgs(macro_args);
			out() << "))";
			++isInMacro;
		}
	}

	struct RelationInfo
	{
		bool hasOpEqual = false;
		bool hasOpLess = false;
	};
	using ClassInfo = std::unordered_map<Type const*, RelationInfo>;

public:
	explicit VisitorToD(
	  ASTContext* Context,
	  MatchContainer const& receiver,
	  StringRef file)
		: Context(Context)
		, receiver(receiver)
		, modulename(llvm::sys::path::stem(file))
	{
	}

	std::string indent_str() const
	{
		return std::string(indent * 4, ' ');
	}

	bool TraverseTranslationUnitDecl(TranslationUnitDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		//SourceManager& sm = Context->getSourceManager();
		//SourceLocation prevDeclEnd;
		//for(auto c: Context->Comments.getComments())
		//	out() << c->getRawText(sm).str() << std::endl;

		for(auto c : Decl->decls())
		{
			if(checkFilename(c))
			{
				pushStream();
				TraverseDecl(c);
				std::string const decl = popStream();
				if(not decl.empty())
				{
					printCommentBefore(c);
					out() << indent_str() << decl;
					if(needSemiComma(c))
						out() << ';';
					printCommentAfter(c);
					out() << std::endl << std::endl;
				}
				output_enabled = (isInMacro == 0);
			}
		}

		return true;
	}

	bool TraverseTypedefDecl(TypedefDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "alias " << mangleName(Decl->getNameAsString()) << " = ";
		PrintType(Decl->getUnderlyingType());
		return true;
	}

	bool TraverseTypeAliasDecl(TypeAliasDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "alias " << mangleName(Decl->getNameAsString()) << " = ";
		PrintType(Decl->getUnderlyingType());
		return true;
	}

	bool TraverseTypeAliasTemplateDecl(TypeAliasTemplateDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "alias " << mangleName(Decl->getNameAsString());
		PrintTemplateParameterList(Decl->getTemplateParameters(), "");
		out() << " = ";
		PrintType(Decl->getTemplatedDecl()->getUnderlyingType());
		return true;
	}

	bool TraverseFieldDecl(FieldDecl* Decl)
	{
		std::string const varName = Decl->getNameAsString();
		if(varName.find("CPP2D_MACRO_STMT") == 0)
		{
			PrintStmtMacro(varName, Decl->getInClassInitializer());
			return true;
		}

		if(pass_decl(Decl)) return true;
		if(Decl->isMutable())
			out() << "/*mutable*/";
		if(Decl->isBitField())
		{
			out() << "\t";
			PrintType(Decl->getType());
			out() << ", \"" << mangleName(varName) << "\", ";
			TraverseStmt(Decl->getBitWidth());
			out() << ',';
			extern_includes.insert("std.bitmanip");
		}
		else
		{
			PrintType(Decl->getType());
			out() << " " << mangleName(Decl->getNameAsString());
		}
		if(Decl->hasInClassInitializer())
		{
			out() << " = ";
			TraverseStmt(Decl->getInClassInitializer());
		}
		return true;
	}

	bool TraverseDependentNameType(DependentNameType* Type)
	{
		TraverseNestedNameSpecifier(Type->getQualifier());
		out() << Type->getIdentifier()->getName().str();
		return true;
	}

	bool TraverseAttributedType(AttributedType* Type)
	{
		PrintType(Type->getEquivalentType());
		return true;
	}

	bool TraverseDecayedType(DecayedType* Type)
	{
		PrintType(Type->getOriginalType());
		return true;
	}

	bool TraverseElaboratedType(ElaboratedType* Type)
	{
		if(Type->getQualifier())
			TraverseNestedNameSpecifier(Type->getQualifier());
		PrintType(Type->getNamedType());
		return true;
	}

	bool TraverseInjectedClassNameType(InjectedClassNameType* Type)
	{
		PrintType(Type->getInjectedSpecializationType());
		return true;
	}

	bool TraverseSubstTemplateTypeParmType(SubstTemplateTypeParmType*)
	{
		return true;
	}

	bool TraverseNestedNameSpecifier(NestedNameSpecifier* NNS)
	{
		if(NNS->getPrefix())
			TraverseNestedNameSpecifier(NNS->getPrefix());

		NestedNameSpecifier::SpecifierKind const kind = NNS->getKind();
		switch(kind)
		{
		//case NestedNameSpecifier::Namespace:
		//case NestedNameSpecifier::NamespaceAlias:
		//case NestedNameSpecifier::Global:
		//case NestedNameSpecifier::Super:
		case NestedNameSpecifier::TypeSpec:
		case NestedNameSpecifier::TypeSpecWithTemplate:
			PrintType(QualType(NNS->getAsType(), 0));
			out() << ".";
			break;
		case NestedNameSpecifier::Identifier:
			out() << NNS->getAsIdentifier()->getName().str() << ".";
			break;
		}
		return true;
	}

	void printTmpArgList(std::string const& tmpArgListStr)
	{
		out() << '!';
		if(tmpArgListStr.find_first_of("!,(") != std::string::npos)
			out() << '(' << tmpArgListStr << ')';
		else
			out() << tmpArgListStr;
	}

	bool TraverseTemplateSpecializationType(TemplateSpecializationType* Type)
	{
		if(isStdArray(Type->desugar()))
		{
			PrintTemplateArgument(Type->getArg(0));
			out() << '[';
			PrintTemplateArgument(Type->getArg(1));
			out() << ']';
			return true;
		}
		else if(isStdUnorderedMap(Type->desugar()))
		{
			PrintTemplateArgument(Type->getArg(1));
			out() << '[';
			PrintTemplateArgument(Type->getArg(0));
			out() << ']';
			return true;
		}
		out() << mangleType(Type->getTemplateName().getAsTemplateDecl());
		auto const argNum = Type->getNumArgs();
		Spliter spliter(", ");
		pushStream();
		for(unsigned int i = 0; i < argNum; ++i)
		{
			spliter.split();
			PrintTemplateArgument(Type->getArg(i));
		}
		printTmpArgList(popStream());
		return true;
	}

	bool TraverseTypedefType(TypedefType* Type)
	{
		out() << mangleType(Type->getDecl());
		return true;
	}

	template<typename InitList>
	bool TraverseCompoundStmtImpl(CompoundStmt* Stmt, InitList initList)
	{
		SourceLocation locStart = Stmt->getLBracLoc().getLocWithOffset(1);
		out() << "{";
		++indent;
		initList();
		for(auto child : Stmt->children())
		{
			printStmtComment(locStart,
			                 child->getLocStart().getLocWithOffset(-1),
			                 child->getLocEnd());
			out() << indent_str();
			TraverseStmt(child);
			if(needSemiComma(child))
				out() << ";";
			output_enabled = (isInMacro == 0);
		}
		printStmtComment(locStart, Stmt->getRBracLoc().getLocWithOffset(-1));
		--indent;
		out() << indent_str();
		out() << "}";
		return true;
	}

	bool TraverseCompoundStmt(CompoundStmt* Stmt)
	{
		return TraverseCompoundStmtImpl(Stmt, [] {});
	}

	template<typename InitList>
	bool TraverseCXXTryStmtImpl(CXXTryStmt* Stmt, InitList initList)
	{
		out() << "try" << std::endl << indent_str();
		auto tryBlock = Stmt->getTryBlock();
		TraverseCompoundStmtImpl(tryBlock, initList);
		auto handlerCount = Stmt->getNumHandlers();
		for(decltype(handlerCount) i = 0; i < handlerCount; ++i)
		{
			out() << std::endl << indent_str();
			TraverseStmt(Stmt->getHandler(i));
		}
		return true;
	}

	bool TraverseCXXTryStmt(CXXTryStmt* Stmt)
	{
		return TraverseCXXTryStmtImpl(Stmt, [] {});
	}

	bool pass_decl(Decl* decl)
	{
		if(receiver.dont_print_this_decl.count(decl))
			return true;
		else
			return false;
	}

	bool TraverseNamespaceDecl(NamespaceDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "// -> module " << mangleName(Decl->getNameAsString()) << ';' << std::endl;
		for(auto decl : Decl->decls())
		{
			pushStream();
			TraverseDecl(decl);
			std::string const declstr = popStream();
			if(not declstr.empty())
			{
				printCommentBefore(decl);
				out() << indent_str() << declstr;
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

	bool TraverseCXXCatchStmt(CXXCatchStmt* Stmt)
	{
		out() << "catch";
		if(Stmt->getExceptionDecl())
		{
			out() << '(';
			TraverseVarDeclImpl(Stmt->getExceptionDecl());
			out() << ')';
		}
		out() << std::endl;
		out() << indent_str();
		TraverseStmt(Stmt->getHandlerBlock());
		return true;
	}

	bool TraverseAccessSpecDecl(AccessSpecDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return true;
	}

	void printBasesClass(CXXRecordDecl* decl)
	{
		Spliter splitBase(", ");
		if(decl->getNumBases() + decl->getNumVBases() != 0)
		{
			out() << " : ";
			auto printBaseSpec = [&](CXXBaseSpecifier & base)
			{
				splitBase.split();
				AccessSpecifier const as = base.getAccessSpecifier();
				if(as != AccessSpecifier::AS_public)
				{
					llvm::errs()
					    << "use of base class protection private and protected is no supported\n";
					out() << "/*" << AccessSpecifierStr[as] << "*/ ";
				}
				PrintType(base.getType());
			};
			for(CXXBaseSpecifier& base : decl->bases())
				printBaseSpec(base);
			for(CXXBaseSpecifier& base : decl->vbases())
				printBaseSpec(base);
		}
	}

	bool TraverseCXXRecordDecl(CXXRecordDecl* decl)
	{
		if(pass_decl(decl)) return true;
		return TraverseCXXRecordDeclImpl(decl, [] {}, [this, decl] {printBasesClass(decl); });
	}

	bool TraverseRecordDecl(RecordDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseCXXRecordDeclImpl(Decl, [] {}, [] {});
	}

	template<typename TmpSpecFunc, typename PrintBasesClass>
	bool TraverseCXXRecordDeclImpl(
	  RecordDecl* decl,
	  TmpSpecFunc traverseTmpSpecs,
	  PrintBasesClass printBasesClass)
	{
		if(decl->isCompleteDefinition() == false && decl->getDefinition() != nullptr)
			return true;

		const bool isClass = decl->isClass();// || decl->isPolymorphic();
		char const* struct_class = decl->isClass() ? "class" : "struct";
		/*if(decl->isCompleteDefinition())
		{
			size_t field_count = std::distance(decl->field_begin(), decl->field_end());
			if(field_count == 0
			   && decl->getKind() == Decl::Kind::CXXRecord)
			{
				CXXRecordDecl* cppRecord = static_cast<CXXRecordDecl*>(decl);

				if(cppRecord->hasUserDeclaredConstructor() == false)
				{
					bool hasBody = false;
					for(CXXMethodDecl* method : cppRecord->methods())
					{
						if(method->isUserProvided() && method->isPure() == false)
						{
							hasBody = true;
							break;
						}
					}
					if(hasBody == false)
						struct_class = "interface";
				}
			}
		}*/
		out() << struct_class << " " << mangleName(decl->getNameAsString());
		traverseTmpSpecs();
		if(decl->isCompleteDefinition() == false)
			return true;
		printBasesClass();
		out() << std::endl << indent_str() << "{";
		++indent;

		auto isBitField = [this](Decl * decl2) -> int
		{
			if(FieldDecl* field = llvm::dyn_cast<FieldDecl>(decl2))
			{
				if(field->isBitField())
					return field->getBitWidthValue(*Context);
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
			  bit_count <= 32 ? 32 :
			  64;
		};

		//Base::TraverseCXXRecordDecl(decl);
		int bit_count = 0;
		bool inBitField = false;
		AccessSpecifier access = isClass ?
		                         AccessSpecifier::AS_private :
		                         AccessSpecifier::AS_public;
		for(Decl* decl2 : decl->decls())
		{
			pushStream();
			int const bc = isBitField(decl2);
			bool const nextIsBitField = (bc >= 0);
			if(nextIsBitField)
				bit_count += bc;
			else if(bit_count != 0)
				out() << "\tuint, \"\", " << (roundPow2(bit_count) - bit_count) << "));\n" << indent_str();
			TraverseDecl(decl2);
			std::string const declstr = popStream();
			if(not declstr.empty())
			{
				AccessSpecifier newAccess = decl2->getAccess();
				if(newAccess == AccessSpecifier::AS_none)
					newAccess = AccessSpecifier::AS_public;
				if(newAccess != access && (isInMacro == 0))
				{
					--indent;
					out() << std::endl << indent_str() << AccessSpecifierStr[newAccess] << ":";
					++indent;
					access = newAccess;
				}
				printCommentBefore(decl2);
				if(inBitField == false && nextIsBitField && (isInMacro == 0))
					out() << "mixin(bitfields!(\n" << indent_str();
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
			out() << "\n" << indent_str() << "\tuint, \"\", " << (roundPow2(bit_count) - bit_count) << "));";
		out() << std::endl;

		//Print all free operator inside the class scope
		auto record_name = decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString();
		for(auto rng = receiver.free_operator.equal_range(record_name);
		    rng.first != rng.second;
		    ++rng.first)
		{
			out() << indent_str();
			TraverseFunctionDeclImpl(const_cast<FunctionDecl*>(rng.first->second), 0);
			out() << std::endl;
		}
		for(auto rng = receiver.free_operator_right.equal_range(record_name);
		    rng.first != rng.second;
		    ++rng.first)
		{
			out() << indent_str();
			TraverseFunctionDeclImpl(const_cast<FunctionDecl*>(rng.first->second), 1);
			out() << std::endl;
		}

		// print the opCmd operator
		if (auto* cxxRecordDecl = dyn_cast<CXXRecordDecl>(decl))
		{
			errs() << cxxRecordDecl->getNameAsString() << " TOTO \n";
			for (auto&& type_info : classInfoMap[cxxRecordDecl])
			{
				Type const* type = type_info.first;
				RelationInfo& info = type_info.second;
				if (info.hasOpLess and info.hasOpEqual)
				{
					out() << indent_str() << "int opCmp(ref in ";
					PrintType(type->getPointeeType());
					out() << " other) const\n";
					out() << indent_str() << "{\n";
					++indent;
					out() << indent_str() << "return _opLess(other) ? -1: ((this == other)? 0: 1);\n";
					--indent;
					out() << indent_str() << "}\n";
				}
			}
		}

		--indent;
		out() << indent_str() << "}";

		return true;
	}

	void PrintTemplateParameterList(TemplateParameterList* tmpParams, std::string const& prevTmplParmsStr)
	{
		out() << "(";
		Spliter spliter1(", ");
		if(prevTmplParmsStr.empty() == false)
		{
			spliter1.split();
			out() << prevTmplParmsStr;
		}
		for(unsigned int i = 0, size = tmpParams->size(); i != size; ++i)
		{
			spliter1.split();
			NamedDecl* param = tmpParams->getParam(i);
			TraverseDecl(param);
			// Print default template arguments
			if(auto* FTTP = dyn_cast<TemplateTypeParmDecl>(param))
			{
				if(FTTP->hasDefaultArgument())
				{
					out() << " = ";
					PrintType(FTTP->getDefaultArgument());
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
					PrintTemplateArgument(FTTTP->getDefaultArgument().getArgument());
				}
			}
		}
		out() << ')';
	}

	bool TraverseClassTemplateDecl(ClassTemplateDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		TraverseCXXRecordDeclImpl(Decl->getTemplatedDecl(), [Decl, this]
		{
			PrintTemplateParameterList(Decl->getTemplateParameters(), "");
		},
		[this, Decl] {printBasesClass(Decl->getTemplatedDecl()); });
		return true;
	}

	TemplateParameterList* getTemplateParameters(ClassTemplateSpecializationDecl*)
	{
		return nullptr;
	}

	TemplateParameterList* getTemplateParameters(ClassTemplatePartialSpecializationDecl* Decl)
	{
		return Decl->getTemplateParameters();
	}

	bool TraverseClassTemplatePartialSpecializationDecl(
	  ClassTemplatePartialSpecializationDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseClassTemplateSpecializationDeclImpl(Decl);
	}

	bool TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseClassTemplateSpecializationDeclImpl(Decl);
	}

	void PrintTemplateArgument(TemplateArgument const& ta)
	{
		switch(ta.getKind())
		{
		case TemplateArgument::Null: break;
		case TemplateArgument::Declaration: TraverseDecl(ta.getAsDecl()); break;
		case TemplateArgument::Integral: out() << ta.getAsIntegral().toString(10); break;
		case TemplateArgument::NullPtr: out() << "null"; break;
		case TemplateArgument::Type: PrintType(ta.getAsType()); break;
		default: TraverseTemplateArgument(ta);
		}
	}

	void PrintTemplateSpec_TmpArgsAndParms(
	  TemplateParameterList& primaryTmpParams,
	  TemplateArgumentList const& tmpArgs,
	  TemplateParameterList* newTmpParams,
	  std::string const& prevTmplParmsStr
	)
	{
		assert(tmpArgs.size() == primaryTmpParams.size());
		out() << '(';
		Spliter spliter2(", ");
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

		for(decltype(tmpArgs.size()) i = 0, size = tmpArgs.size(); i != size; ++i)
		{
			spliter2.split();
			renameIdentifiers = false;
			TraverseDecl(primaryTmpParams.getParam(i));
			renameIdentifiers = true;
			out() << " : ";
			PrintTemplateArgument(tmpArgs.get(i));
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
	bool TraverseClassTemplateSpecializationDeclImpl(D* Decl)
	{
		if(pass_decl(Decl)) return true;
		if(Decl->getSpecializationKind() == TSK_ExplicitInstantiationDeclaration
		   || Decl->getSpecializationKind() == TSK_ExplicitInstantiationDefinition
		   || Decl->getSpecializationKind() == TSK_ImplicitInstantiation)
			return true;

		template_args_stack.emplace_back();
		TemplateParameterList* tmpParams = getTemplateParameters(Decl);
		if(tmpParams)
		{
			auto& template_args = template_args_stack.back();
			for(decltype(tmpParams->size()) i = 0, size = tmpParams->size(); i != size; ++i)
				template_args.push_back(tmpParams->getParam(i));
		}
		TemplateParameterList& specializedTmpParams = *Decl->getSpecializedTemplate()->getTemplateParameters();
		TemplateArgumentList const& tmpArgs = Decl->getTemplateArgs();
		TraverseCXXRecordDeclImpl(Decl, [&]
		{
			PrintTemplateSpec_TmpArgsAndParms(specializedTmpParams, tmpArgs, tmpParams, "");
		},
		[this, Decl] {printBasesClass(Decl); });
		template_args_stack.pop_back();
		return true;
	}

	bool TraverseCXXConversionDecl(CXXConversionDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseFunctionDeclImpl(Decl);
	}

	bool TraverseCXXConstructorDecl(CXXConstructorDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseFunctionDeclImpl(Decl);
	}

	bool TraverseCXXDestructorDecl(CXXDestructorDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseFunctionDeclImpl(Decl);
	}

	bool TraverseCXXMethodDecl(CXXMethodDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		if(Decl->getLexicalParent() == Decl->getParent())
			return TraverseFunctionDeclImpl(Decl);
		else
			return true;
	}

	bool TraversePredefinedExpr(PredefinedExpr*)
	{
		out() << "__PRETTY_FUNCTION__";
		return true;
	}

	bool TraverseCXXDefaultArgExpr(CXXDefaultArgExpr*)
	{
		return true;
	}

	bool TraverseCXXUnresolvedConstructExpr(CXXUnresolvedConstructExpr*  Expr)
	{
		PrintType(Expr->getTypeAsWritten());
		Spliter spliter(", ");
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

	bool TraverseUnresolvedLookupExpr(UnresolvedLookupExpr*  Expr)
	{
		out() << mangleName(Expr->getName().getAsString());
		if(Expr->hasExplicitTemplateArgs())
		{
			auto const argNum = Expr->getNumTemplateArgs();
			Spliter spliter(", ");
			pushStream();
			for(size_t i = 0; i < argNum; ++i)
			{
				spliter.split();
				auto tmpArg = Expr->getTemplateArgs()[i];
				PrintTemplateArgument(tmpArg.getArgument());
			}
			printTmpArgList(popStream());
		}
		return true;
	}

	bool TraverseCXXForRangeStmt(CXXForRangeStmt*  Stmt)
	{
		out() << "foreach(";
		refAccepted = true;
		inForRangeInit = true;
		TraverseVarDeclImpl(dyn_cast<VarDecl>(Stmt->getLoopVarStmt()->getSingleDecl()));
		inForRangeInit = false;
		refAccepted = false;
		out() << "; ";
		TraverseStmt(Stmt->getRangeInit());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		return true;
	}

	bool TraverseDoStmt(DoStmt*  Stmt)
	{
		out() << "do" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		out() << "while(";
		TraverseStmt(Stmt->getCond());
		out() << ")";
		return true;
	}

	bool TraverseSwitchStmt(SwitchStmt* Stmt)
	{
		out() << "switch(";
		TraverseStmt(Stmt->getCond());
		out() << ")" << std::endl << indent_str();
		TraverseStmt(Stmt->getBody());
		return true;
	}

	bool TraverseCaseStmt(CaseStmt* Stmt)
	{
		out() << "case ";
		TraverseStmt(Stmt->getLHS());
		out() << ":" << std::endl;
		++indent;
		out() << indent_str();
		TraverseStmt(Stmt->getSubStmt());
		--indent;
		return true;
	}

	bool TraverseBreakStmt(BreakStmt*)
	{
		out() << "break";
		return true;
	}

	bool TraverseStaticAssertDecl(StaticAssertDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "static assert(";
		TraverseStmt(Decl->getAssertExpr());
		out() << ", ";
		TraverseStmt(Decl->getMessage());
		out() << ")";
		return true;
	}

	bool TraverseDefaultStmt(DefaultStmt* Stmt)
	{
		out() << "default:" << std::endl;
		++indent;
		out() << indent_str();
		TraverseStmt(Stmt->getSubStmt());
		--indent;
		return true;
	}

	bool TraverseCXXDeleteExpr(CXXDeleteExpr* Expr)
	{
		TraverseStmt(Expr->getArgument());
		out() << " = null";
		return true;
	}

	bool TraverseCXXNewExpr(CXXNewExpr* Expr)
	{
		out() << "new ";
		if(Expr->isArray())
		{
			PrintType(Expr->getAllocatedType());
			out() << '[';
			TraverseStmt(Expr->getArraySize());
			out() << ']';
		}
		else
		{
			switch(Expr->getInitializationStyle())
			{
			case CXXNewExpr::NoInit:
				PrintType(Expr->getAllocatedType());
				break;
			case CXXNewExpr::CallInit:
				PrintType(Expr->getAllocatedType());
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

	void PrintCXXConstructExprParams(CXXConstructExpr* Init)
	{
		if (Init->getNumArgs() == 1)  //Handle Copy ctor
		{
			QualType recordType = Init->getType();
			recordType.addConst();
			if (recordType == Init->getArg(0)->getType())
			{
				TraverseStmt(Init->getArg(0));
				return;
			}
		}
		PrintType(Init->getType());
		out() << '(';
		Spliter spliter(", ");
		for(auto arg : Init->arguments())
		{
			if(arg->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(arg);
			}
		}
		out() << ')';
	}

	bool TraverseCXXConstructExpr(CXXConstructExpr* Init)
	{
		Spliter spliter(", ");
		for (unsigned i = 0, e = Init->getNumArgs(); i != e; ++i) 
		{
			if (isa<CXXDefaultArgExpr>(Init->getArg(i)))
				break; // Don't print any defaulted arguments

			spliter.split();
			TraverseStmt(Init->getArg(i));
		}

		return true;
	}

	void PrintType(QualType const& type)
	{
		if(type.getTypePtr()->getTypeClass() == Type::TypeClass::Auto)
		{
			if(type.isConstQualified())
				out() << "const ";
			TraverseType(type);
		}
		else
		{
			if(type.isConstQualified())
				out() << "const(";
			TraverseType(type);
			if(type.isConstQualified())
				out() << ')';
		}
	}

	bool TraverseConstructorInitializer(CXXCtorInitializer* Init)
	{
		if(Init->getMember())
			out() << mangleName(Init->getMember()->getNameAsString());
		if(TypeSourceInfo* TInfo = Init->getTypeSourceInfo())
			PrintType(TInfo->getType());
		out() << " = ";
		TraverseStmt(Init->getInit());

		if(Init->getNumArrayIndices() && getDerived().shouldVisitImplicitCode())
			for(VarDecl* VD : Init->getArrayIndexes())
				TraverseDecl(VD);
		return true;
	}


	void startCtorBody(FunctionDecl*) {}

	void startCtorBody(CXXConstructorDecl* Decl)
	{
		auto ctor_init_count = Decl->getNumCtorInitializers();
		if(ctor_init_count != 0)
		{
			for(auto init : Decl->inits())
			{
				if(init->isWritten())
				{
					out() << std::endl << indent_str();
					TraverseConstructorInitializer(init);
					out() << ";";
				}
			}
		}
	}

	void printFuncEnd(CXXMethodDecl* Decl)
	{
		if(Decl->isConst())
			out() << " const";
	}

	void printFuncEnd(FunctionDecl*) {}

	void printSpecialMethodAttribute(CXXMethodDecl* Decl)
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

	bool printFuncBegin(CXXMethodDecl* Decl, std::string& tmpParams, int arg_become_this = -1)
	{
		if(Decl->isOverloadedOperator()
		   && Decl->getOverloadedOperator() == OverloadedOperatorKind::OO_ExclaimEqual)
			return false;
		printSpecialMethodAttribute(Decl);
		printFuncBegin((FunctionDecl*)Decl, tmpParams, arg_become_this);
		return true;
	}

	bool printFuncBegin(FunctionDecl* Decl, std::string& tmpParams, int arg_become_this = -1)
	{
		if(Decl->isOverloadedOperator()
		   && Decl->getOverloadedOperator() == OverloadedOperatorKind::OO_ExclaimEqual)
			return false;
		std::string const name = Decl->getNameAsString();
		if(name == "cpp2d_dummy_variadic")
			return false;
		PrintType(Decl->getReturnType());
		out() << " ";
		if(Decl->isOverloadedOperator())
		{
			QualType arg1Type;
			QualType arg2Type;
			CXXRecordDecl* arg1Record = nullptr;
			CXXRecordDecl* arg2Record = nullptr;
			if (auto* methodDecl = dyn_cast<CXXMethodDecl>(Decl))
			{
				arg1Type = methodDecl->getThisType(*Context);
				arg1Record = methodDecl->getParent();
				if (methodDecl->getNumParams() > 0)
				{
					arg2Type = methodDecl->getParamDecl(0)->getType();
					arg2Record = arg2Type->getAsCXXRecordDecl();
				}
			}
			else
			{
				if (methodDecl->getNumParams() > 0)
				{
					arg1Type = Decl->getParamDecl(0)->getType();
					arg1Record = arg1Type->getAsCXXRecordDecl();
				}
				if (methodDecl->getNumParams() > 1)
				{
					arg2Type = Decl->getParamDecl(1)->getType();
					arg2Record = arg2Type->getAsCXXRecordDecl();
				}
			}
			size_t const nbArgs = (arg_become_this == -1 ? 1 : 0) + Decl->getNumParams();
			std::string const right = (arg_become_this == 1) ? "Right" : "";
			OverloadedOperatorKind const opKind = Decl->getOverloadedOperator();
			if (opKind == OverloadedOperatorKind::OO_EqualEqual)
			{
				out() << "opEquals" + right;
				if (arg1Record)
				{
					classInfoMap[arg1Record][arg2Type.getTypePtr()].hasOpEqual = true;
					errs() << arg1Record->getNameAsString() << " has opEquals\n";
				}
				if (arg2Record)
				{
					classInfoMap[arg2Record][arg1Type.getTypePtr()].hasOpEqual = true;
					errs() << arg2Record->getNameAsString() << " has opEquals\n";
				}
			}
			else if(opKind == OverloadedOperatorKind::OO_Call)
				out() << "opCall" + right;
			else if(opKind == OverloadedOperatorKind::OO_Subscript)
				out() << "opIndex" + right;
			else if(opKind == OverloadedOperatorKind::OO_Equal)
				out() << "opAssign" + right;
			else if (opKind == OverloadedOperatorKind::OO_Less)
			{
				out() << "_opLess" + right;
				if (arg1Record)
				{
					classInfoMap[arg1Record][arg2Type.getTypePtr()].hasOpLess = true;
					errs() << arg1Record->getNameAsString() << " has opLess\n";
				}
				if (arg2Record)
				{
					classInfoMap[arg2Record][arg1Type.getTypePtr()].hasOpLess = true;
					errs() << arg2Record->getNameAsString() << " has opLess\n";
				}
			}
			else if (opKind == OverloadedOperatorKind::OO_LessEqual)
				out() << "_opLessEqual" + right;
			else if (opKind == OverloadedOperatorKind::OO_Greater)
				out() << "_opGreater" + right;
			else if (opKind == OverloadedOperatorKind::OO_GreaterEqual)
				out() << "_opGreaterEqual" + right;
			else if (opKind == OverloadedOperatorKind::OO_PlusPlus && nbArgs == 2)
				out() << "_opPostPlusplus" + right;
			else if (opKind == OverloadedOperatorKind::OO_MinusMinus && nbArgs == 2)
				out() << "_opPostMinusMinus" + right;
			else
			{
				std::string spelling = getOperatorSpelling(opKind);
				if (nbArgs == 1)
					out() << "opUnary" + right;
				else  // Two args
				{
					if (spelling.back() == '=') //Handle self assign operators
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

	bool printFuncBegin(CXXConversionDecl* Decl, std::string& tmpParams, int = -1)
	{
		printSpecialMethodAttribute(Decl);
		PrintType(Decl->getConversionType());
		out() << " opCast";
		pushStream();
		out() << "T : ";
		PrintType(Decl->getConversionType());
		tmpParams = popStream();
		return true;
	}

	bool printFuncBegin(CXXConstructorDecl* Decl,
	                    std::string&,	//tmpParams
	                    int = -1		//arg_become_this = -1
	                   )
	{
		auto record = Decl->getParent();
		if(record->isStruct() && Decl->getNumParams() == 0)
			return false; //If default struct ctor : don't print
		out() << "this";
		return true;
	}

	bool printFuncBegin(CXXDestructorDecl*,
	                    std::string&,	//tmpParams,
	                    int = -1		//arg_become_this = -1
	                   )
	{
		//if(Decl->isPure() && !Decl->hasBody())
		//	return false; //ctor and dtor can't be abstract
		//else
		out() << "~this";
		return true;
	}


	template<typename D>
	bool TraverseFunctionDeclImpl(
	  D* Decl,
	  int arg_become_this = -1)
	{
		if(Decl->doesThisDeclarationHaveABody() == false && Decl->hasBody())
			return false;

		refAccepted = true;
		std::string tmplParamsStr;
		if(printFuncBegin(Decl, tmplParamsStr, arg_become_this) == false)
		{
			refAccepted = false;
			return true;
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
				PrintTemplateParameterList(tDecl->getTemplateParameters(), tmplParamsStr);
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
				PrintTemplateSpec_TmpArgsAndParms(*primaryTmpParams, *tmpArgs, nullptr, tmplParamsStr);
				tmplPrinted = true;
			}
			break;
		default: assert(false && "Inconststent clang::FunctionDecl::TemplatedKind");
		}
		if(not tmplPrinted and not tmplParamsStr.empty())
			out() << '(' << tmplParamsStr << ')';
		out() << "(";
		if(Decl->getNumParams() != 0)
		{
			TypeSourceInfo* declSourceInfo = Decl->getTypeSourceInfo();
			TypeLoc declTypeLoc = declSourceInfo->getTypeLoc();
			FunctionTypeLoc funcTypeLoc;
			SourceLocation locStart;
			TypeLoc::TypeLocClass tlClass = declTypeLoc.getTypeLocClass();
			if(tlClass == TypeLoc::TypeLocClass::FunctionProto)
			{
				funcTypeLoc = declTypeLoc.castAs<FunctionTypeLoc>();
				locStart = funcTypeLoc.getLParenLoc().getLocWithOffset(1);
			}
			++indent;
			size_t index = 0;
			size_t const numParam =
			  Decl->getNumParams() +
			  (Decl->isVariadic() ? 1 : 0) +
			  ((arg_become_this == -1) ? 0 : -1);
			for(auto decl : Decl->params())
			{
				if(arg_become_this != index)
				{
					if(numParam != 1)
					{
						printStmtComment(locStart,
						                 decl->getLocStart().getLocWithOffset(-1),
						                 decl->getLocEnd().getLocWithOffset(1));
						out() << indent_str();
					}
					TraverseDecl(decl);
					if(index != numParam - 1)
						out() << ',';
				}
				++index;
			}
			if(Decl->isVariadic())
			{
				if(numParam != 1)
					out() << "\n" << indent_str();
				out() << "...";
			}
			pushStream();
			if(funcTypeLoc.isNull() == false)
				printStmtComment(locStart, funcTypeLoc.getRParenLoc());
			std::string const comment = popStream();
			--indent;
			if(comment.size() > 2)
				out() << comment << indent_str();
		}
		out() << ")";
		printFuncEnd(Decl);
		refAccepted = false;
		if(Stmt* body = Decl->getBody())
		{
			//Stmt* body = Decl->getBody();
			out() << std::endl << std::flush;
			auto alias_this = [Decl, arg_become_this, this]
			{
				if(arg_become_this >= 0)
				{
					ParmVarDecl* param = *(Decl->param_begin() + arg_become_this);
					out() << std::endl;
					out() << indent_str() << "alias " << param->getNameAsString() << " = this;";
				}
			};
			if(body->getStmtClass() == Stmt::CXXTryStmtClass)
			{
				out() << indent_str() << '{' << std::endl;
				++indent;
				out() << indent_str();
				TraverseCXXTryStmtImpl(static_cast<CXXTryStmt*>(body),
				                       [&] {startCtorBody(Decl); alias_this(); });
				out() << std::endl;
				--indent;
				out() << indent_str() << '}';
			}
			else
			{
				out() << indent_str();
				assert(body->getStmtClass() == Stmt::CompoundStmtClass);
				TraverseCompoundStmtImpl(static_cast<CompoundStmt*>(body),
				                         [&] {startCtorBody(Decl); alias_this(); });
			}
		}
		else
			out() << ";";

		return true;
	}

	bool TraverseUsingDecl(UsingDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "//using " << Decl->getNameAsString();
		return true;
	}

	bool TraverseFunctionDecl(FunctionDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return TraverseFunctionDeclImpl(Decl);
	}

	bool TraverseUsingDirectiveDecl(UsingDirectiveDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return true;
	}


	bool TraverseFunctionTemplateDecl(FunctionTemplateDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		FunctionDecl* FDecl = Decl->getTemplatedDecl();
		switch(FDecl->getKind())
		{
		case Decl::Function:
			return TraverseFunctionDeclImpl(FDecl);
		case Decl::CXXMethod:
			return TraverseFunctionDeclImpl(llvm::cast<CXXMethodDecl>(FDecl));
		case Decl::CXXConstructor:
			return TraverseFunctionDeclImpl(llvm::cast<CXXConstructorDecl>(FDecl));
		case Decl::CXXConversion:
			return TraverseFunctionDeclImpl(llvm::cast<CXXConversionDecl>(FDecl));
		case Decl::CXXDestructor:
			return TraverseFunctionDeclImpl(llvm::cast<CXXDestructorDecl>(FDecl));
		default: assert(false && "Inconsistent FunctionDecl kind in FunctionTemplateDecl");
			return true;
		}
	}

	bool TraverseBuiltinType(BuiltinType* Type)
	{
		//PrintingPolicy pp = LangOptions();
		//pp.Bool = 1;
		//out() << Type->getNameAsCString(pp);
		out() << [Type]
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
			case BuiltinType::OCLImage1d: return "image1d_t";
			case BuiltinType::OCLImage1dArray: return "image1d_array_t";
			case BuiltinType::OCLImage1dBuffer: return "image1d_buffer_t";
			case BuiltinType::OCLImage2d: return "image2d_t";
			case BuiltinType::OCLImage2dArray: return "image2d_array_t";
			case BuiltinType::OCLImage2dDepth: return "image2d_depth_t";
			case BuiltinType::OCLImage2dArrayDepth: return "image2d_array_depth_t";
			case BuiltinType::OCLImage2dMSAA: return "image2d_msaa_t";
			case BuiltinType::OCLImage2dArrayMSAA: return "image2d_array_msaa_t";
			case BuiltinType::OCLImage2dMSAADepth: return "image2d_msaa_depth_t";
			case BuiltinType::OCLImage2dArrayMSAADepth: return "image2d_array_msaa_depth_t";
			case BuiltinType::OCLImage3d: return "image3d_t";
			case BuiltinType::OCLSampler: return "sampler_t";
			case BuiltinType::OCLEvent: return "event_t";
			case BuiltinType::OCLClkEvent: return "clk_event_t";
			case BuiltinType::OCLQueue: return "queue_t";
			case BuiltinType::OCLNDRange: return "ndrange_t";
			case BuiltinType::OCLReserveID: return "reserve_id_t";
			case BuiltinType::OMPArraySection: return "<OpenMP array section type>";
			default: assert(false && "invalid Type->getKind()");
			}
			return "";
		}();
		return true;
	}

	enum Semantic
	{
		Value,
		Reference,
	};
	Semantic getSemantic(QualType qt)
	{
		Type const* type = qt.getTypePtr();
		return type->isClassType() || type->isFunctionType() ? Reference : Value;
	}

	template<typename PType>
	bool TraversePointerTypeImpl(PType* Type)
	{
		QualType const pointee = Type->getPointeeType();
		Type::TypeClass const tc = pointee->getTypeClass();
		if(tc == Type::Paren)  //function pointer do not need '*'
		{
			auto innerType = static_cast<ParenType const*>(pointee.getTypePtr())->getInnerType();
			if(innerType->getTypeClass() == Type::FunctionProto)
				return TraverseType(innerType);
		}
		PrintType(pointee);
		out() << ((getSemantic(pointee) == Value) ? "[]" : ""); //'*';
		return true;
	}

	bool TraverseMemberPointerType(MemberPointerType* Type)
	{
		return TraversePointerTypeImpl(Type);
	}
	bool TraversePointerType(PointerType* Type)
	{
		return TraversePointerTypeImpl(Type);
	}

	bool TraverseCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr*)
	{
		out() << "null";
		return true;
	}

	bool TraverseEnumConstantDecl(EnumConstantDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << mangleName(Decl->getNameAsString());
		if(Decl->getInitExpr())
		{
			out() << " = ";
			TraverseStmt(Decl->getInitExpr());
		}
		return true;
	}

	bool TraverseEnumDecl(EnumDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "enum " << mangleName(Decl->getNameAsString());
		if(Decl->isFixed())
		{
			out() << " : ";
			TraverseType(Decl->getIntegerType());
		}
		out() << std::endl << indent_str() << "{" << std::endl;
		++indent;
		size_t count = 0;
		for(auto e : Decl->enumerators())
		{
			++count;
			out() << indent_str();
			TraverseDecl(e);
			out() << "," << std::endl;
		}
		if(count == 0)
			out() << indent_str() << "Default" << std::endl;
		--indent;
		out() << indent_str() << "}";
		return true;
	}

	bool TraverseEnumType(EnumType* Type)
	{
		out() << mangleName(Type->getDecl()->getNameAsString());
		return true;
	}

	bool TraverseIntegerLiteral(IntegerLiteral* Stmt)
	{
		out() << Stmt->getValue().toString(10, true);
		return true;
	}

	bool TraverseDecltypeType(DecltypeType* Type)
	{
		out() << "typeof(";
		TraverseStmt(Type->getUnderlyingExpr());
		out() << ')';
		return true;
	}

	bool TraverseAutoType(AutoType*)
	{
		if(not inForRangeInit)
			out() << "auto";
		return true;
	}

	/*bool TraverseMemberPointerType(MemberPointerType *Type)
	{
	//out() << Type->getTypeClassName();
	return true;
	}*/

	bool TraverseLinkageSpecDecl(LinkageSpecDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		switch(Decl->getLanguage())
		{
		case LinkageSpecDecl::LanguageIDs::lang_c: out() << "extern (C) "; break;
		case LinkageSpecDecl::LanguageIDs::lang_cxx: out() << "extern (C++) "; break;
		default: assert(false && "Inconsistant LinkageSpecDecl::LanguageIDs");
		}
		DeclContext* declContext = LinkageSpecDecl::castToDeclContext(Decl);;
		if(Decl->hasBraces())
		{
			out() << "\n" << indent_str() << "{\n";
			++indent;
			for(auto* decl : declContext->decls())
			{
				out() << indent_str();
				TraverseDecl(decl);
				if(needSemiComma(decl))
					out() << ";";
				out() << "\n";
			}
			--indent;
			out() << indent_str() << "}";
		}
		else
			TraverseDecl(*declContext->decls_begin());
		return true;
	}

	bool TraverseFriendDecl(FriendDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		out() << "//friend ";
		if(Decl->getFriendType())
			TraverseType(Decl->getFriendType()->getType());
		else
			TraverseDecl(Decl->getFriendDecl());
		return true;
	}

	bool TraverseParmVarDecl(ParmVarDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		PrintType(Decl->getType());
		std::string const name = Decl->getNameAsString();
		if(name.empty() == false)
			out() <<  " " << mangleName(name);
		if(Decl->hasDefaultArg())
		{
			out() << " = ";
			TraverseStmt(
			  Decl->hasUninstantiatedDefaultArg() ?
			  Decl->getUninstantiatedDefaultArg() :
			  Decl->getDefaultArg());
		}
		return true;
	}

	bool TraverseRValueReferenceType(RValueReferenceType* Type)
	{
		PrintType(Type->getPointeeType());
		out() << "/*&&*/";
		return true;
	}

	bool TraverseLValueReferenceType(LValueReferenceType* Type)
	{
		if(receiver.ref_to_class.count(Type))
			PrintType(Type->getPointeeType());
		else
		{
			if(refAccepted)
			{
				if(getSemantic(Type->getPointeeType()) == Value)
					out() << "ref ";
				PrintType(Type->getPointeeType());
			}
			else
			{
				PrintType(Type->getPointeeType());
				if(getSemantic(Type->getPointeeType()) == Value)
					out() << "[]";
			}
		}
		return true;
	}

	bool TraverseTemplateTypeParmType(TemplateTypeParmType* Type)
	{
		if(Type->getDecl())
			TraverseDecl(Type->getDecl());
		else
		{
			IdentifierInfo* identifier = Type->getIdentifier();
			if(identifier == nullptr)
			{
				auto param = template_args_stack[Type->getDepth()][Type->getIndex()];
				identifier = param->getIdentifier();
				if(identifier == nullptr)
					TraverseDecl(param);
			}
			auto iter = renamedIdentifiers.find(identifier);
			if(iter != renamedIdentifiers.end())
				out() << iter->second;
			else
				out() << identifier->getName().str();
		}
		return true;
	}

	bool TraverseTemplateTypeParmDecl(TemplateTypeParmDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		IdentifierInfo* identifier = Decl->getIdentifier();
		if(identifier)
		{
			auto iter = renamedIdentifiers.find(identifier);
			if(renameIdentifiers && iter != renamedIdentifiers.end())
				out() << iter->second;
			else
				out() << identifier->getName().str();
		}
		return true;
	}

	bool TraverseNonTypeTemplateParmDecl(NonTypeTemplateParmDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		PrintType(Decl->getType());
		out() << " ";
		IdentifierInfo* identifier = Decl->getIdentifier();
		if(identifier)
			out() << mangleName(identifier->getName());
		return true;
	}


	bool TraverseDeclStmt(DeclStmt* Stmt)
	{
		if(Stmt->isSingleDecl()) //May be in for or catch
			TraverseDecl(Stmt->getSingleDecl());
		else
		{
			bool first = true;
			for(auto d : Stmt->decls())
			{
				if(first)
					first = false;
				else
					out() << indent_str();
				TraverseDecl(d);
				out() << ";";
				out() << std::endl;
			}
		}
		return true;
	}

	bool TraverseNamespaceAliasDecl(NamespaceAliasDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return true;
	}

	bool TraverseReturnStmt(ReturnStmt* Stmt)
	{
		out() << "return";
		if (Stmt->getRetValue()) {
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

	bool TraverseCXXOperatorCallExpr(CXXOperatorCallExpr* Stmt)
	{
		auto const numArgs = Stmt->getNumArgs();
		const OverloadedOperatorKind kind = Stmt->getOperator();
		char const* opStr = getOperatorSpelling(kind);
		if(kind == OverloadedOperatorKind::OO_Call || kind == OverloadedOperatorKind::OO_Subscript)
		{
			auto iter = Stmt->arg_begin(), end = Stmt->arg_end();
			TraverseStmt(*iter);
			Spliter spliter(", ");
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

			bool const lo_ptr = lo->getType()->isPointerType();
			bool const ro_ptr = ro->getType()->isPointerType();

			Semantic const lo_sem = getSemantic(lo->getType());
			Semantic const ro_sem = getSemantic(ro->getType());

			bool const dup = //both operands will be transformed to pointer
			  (ro_ptr == false && ro_sem == Reference) &&
			  (lo_ptr == false && lo_sem == Reference);

			if(dup)
			{
				TraverseStmt(lo);
				out() << ".opAssign(";
				TraverseStmt(ro);
				out() << ")";
			}
			else
			{
				TraverseStmt(lo);
				out() << " = ";
				TraverseStmt(ro);
			}
		}
		else if (kind == OverloadedOperatorKind::OO_PlusPlus || kind == OverloadedOperatorKind::OO_MinusMinus)
		{
			if (numArgs == 2)
			{
				TraverseStmt(*Stmt->arg_begin());
				out() << opStr;
			}
			else
			{
				out() << opStr;
				TraverseStmt(*Stmt->arg_begin());
			}
			return true;
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

	bool TraverseExprWithCleanups(ExprWithCleanups* Stmt)
	{
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	void TraverseCompoundStmtOrNot(Stmt* Stmt)
	{
		if(Stmt->getStmtClass() == Stmt::StmtClass::CompoundStmtClass)
		{
			out() << indent_str();
			TraverseStmt(Stmt);
		}
		else
		{
			++indent;
			out() << indent_str();
			TraverseStmt(Stmt);
			if(needSemiComma(Stmt))
				out() << ";";
			--indent;
		}
	}

	bool TraverseArraySubscriptExpr(ArraySubscriptExpr* Expr)
	{
		TraverseStmt(Expr->getLHS());
		out() << '[';
		TraverseStmt(Expr->getRHS());
		out() << ']';
		return true;
	}

	bool TraverseFloatingLiteral(FloatingLiteral* Expr)
	{
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

	bool TraverseForStmt(ForStmt* Stmt)
	{
		out() << "for(";
		TraverseStmt(Stmt->getInit());
		out() << "; ";
		TraverseStmt(Stmt->getCond());
		out() << "; ";
		TraverseStmt(Stmt->getInc());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		return true;
	}

	bool TraverseWhileStmt(WhileStmt* Stmt)
	{
		out() << "while(";
		TraverseStmt(Stmt->getCond());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		return true;
	}

	bool TraverseIfStmt(IfStmt* Stmt)
	{
		out() << "if(";
		TraverseStmt(Stmt->getCond());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getThen());
		if(Stmt->getElse())
		{
			out() << std::endl << indent_str() << "else ";
			if(Stmt->getElse()->getStmtClass() == Stmt::IfStmtClass)
				TraverseStmt(Stmt->getElse());
			else
			{
				out() << std::endl;
				TraverseCompoundStmtOrNot(Stmt->getElse());
			}
		}
		return true;
	}

	bool TraverseCXXBindTemporaryExpr(CXXBindTemporaryExpr* Stmt)
	{
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseCXXThrowExpr(CXXThrowExpr* Stmt)
	{
		out() << "throw ";
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseMaterializeTemporaryExpr(MaterializeTemporaryExpr* Stmt)
	{
		TraverseStmt(Stmt->GetTemporaryExpr());
		return true;
	}

	bool TraverseCXXFunctionalCastExpr(CXXFunctionalCastExpr* Stmt)
	{
		PrintType(Stmt->getTypeInfoAsWritten()->getType());
		out() << '(';
		TraverseStmt(Stmt->getSubExpr());
		out() << ')';
		return true;
	}

	bool TraverseParenType(ParenType* Type)
	{
		// Parenthesis are useless (and illegal) on function types
		PrintType(Type->getInnerType());
		return true;
	}

	bool TraverseFunctionProtoType(FunctionProtoType* Type)
	{
		PrintType(Type->getReturnType());
		out() << " function(";
		Spliter spliter(", ");
		for(auto const& p : Type->getParamTypes())
		{
			spliter.split();
			PrintType(p);
		}
		if(Type->isVariadic())
		{
			spliter.split();
			out() << "...";
		}
		out() << ')';
		return true;
	}

	bool TraverseCXXTemporaryObjectExpr(CXXTemporaryObjectExpr* Stmt)
	{
		TraverseCXXConstructExpr(Stmt);
		return true;
	}

	bool TraverseNullStmt(NullStmt*)
	{
		return true;
	}

	bool TraverseCharacterLiteral(CharacterLiteral* Stmt)
	{
		out() << '\'';
		int c = Stmt->getValue();
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

	bool TraverseStringLiteral(StringLiteral* Stmt)
	{
		out() << "\"";
		std::string literal;
		auto str = Stmt->getString();
		if(Stmt->isUTF16() || Stmt->isWide())
		{
			typedef unsigned short ushort;
			static_assert(sizeof(ushort) == 2, "sizeof(unsigned short) == 2 expected");
			std::basic_string<unsigned short> literal16((ushort*)str.data(), str.size() / 2);
			std::wstring_convert<std::codecvt_utf8<ushort>, ushort> cv;
			literal = cv.to_bytes(literal16);
		}
		else if(Stmt->isUTF32())
		{
			static_assert(sizeof(unsigned int) == 4, "sizeof(unsigned int) == 4 required");
			std::basic_string<unsigned int> literal32((unsigned int*)str.data(), str.size() / 4);
			std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> cv;
			literal = cv.to_bytes(literal32);
		}
		else
			literal = std::string(str.data(), str.size());
		size_t pos = 0;
		while((pos = literal.find('\\', pos)) != std::string::npos)
		{
			literal = literal.substr(0, pos) + "\\\\" + literal.substr(pos + 1);
			pos += 2;
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
		out() << "\\0\"";
		return true;
	}

	bool TraverseCXXBoolLiteralExpr(CXXBoolLiteralExpr* Stmt)
	{
		out() << (Stmt->getValue() ? "true" : "false");
		return true;
	}

	bool TraverseUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr* Expr)
	{
		//out() << '(';
		if(Expr->isArgumentType())
			PrintType(Expr->getArgumentType());
		else
			TraverseStmt(Expr->getArgumentExpr());
		//out() << ')';
		switch(Expr->getKind())
		{
		case UnaryExprOrTypeTrait::UETT_AlignOf:
			out() << ".alignof";
			break;
		case UnaryExprOrTypeTrait::UETT_SizeOf:
			out() << ".sizeof";
			break;
		case UnaryExprOrTypeTrait::UETT_OpenMPRequiredSimdAlign:
			out() << ".OpenMPRequiredSimdAlign";
			break;
		case UnaryExprOrTypeTrait::UETT_VecStep:
			out() << ".VecStep";
			break;
		}
		return true;
	}

	bool TraverseEmptyDecl(EmptyDecl* Decl)
	{
		if(pass_decl(Decl)) return true;
		return true;
	}


	bool TraverseLambdaExpr(LambdaExpr* S)
	{
		TypeLoc TL = S->getCallOperator()->getTypeSourceInfo()->getTypeLoc();
		FunctionProtoTypeLoc Proto = TL.castAs<FunctionProtoTypeLoc>();

		if(S->hasExplicitParameters() && S->hasExplicitResultType())
		{
			out() << '(';
			// Visit the whole type.
			TraverseTypeLoc(TL);
			out() << ')' << std::endl;
		}
		else
		{
			if(S->hasExplicitParameters())
			{
				Spliter spliter2(", ");
				out() << '(';
				refAccepted = true;
				// Visit parameters.
				for(unsigned I = 0, N = Proto.getNumParams(); I != N; ++I)
				{
					spliter2.split();
					TraverseDecl(Proto.getParam(I));
				}
				refAccepted = false;
				out() << ')' << std::endl;
			}
			else if(S->hasExplicitResultType())
			{
				TraverseTypeLoc(Proto.getReturnLoc());
			}

			auto* T = Proto.getTypePtr();
			for(const auto& E : T->exceptions())
			{
				TraverseType(E);
			}

			//if (Expr *NE = T->getNoexceptExpr())
			//	TRY_TO_TRAVERSE_OR_ENQUEUE_STMT(NE);
		}
		out() << indent_str();
		TraverseStmt(S->getBody());
		return true;
		//return TRAVERSE_STMT_BASE(LambdaBody, LambdaExpr, S, Queue);
	}

	std::set<Expr*> dont_take_ptr;

	bool TraverseCallExpr(CallExpr* Stmt)
	{
		Expr* func = Stmt->getCallee();
		dont_take_ptr.insert(func);
		TraverseStmt(func);
		dont_take_ptr.erase(func);
		// Are parentezis on zero argument needed?
		//if (Stmt->getNumArgs() == 0)
		//	return true;
		out() << "(";
		Spliter spliter(", ");
		for(Expr* c : Stmt->arguments())
		{
			if(c->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(c);
			}
		}
		out() << ")";// << std::endl;
		return true;
	}

	bool TraverseImplicitCastExpr(ImplicitCastExpr* Stmt)
	{
		if(Stmt->getCastKind() == CK_FunctionToPointerDecay && dont_take_ptr.count(Stmt) == 0)
			out() << "&";
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseCXXThisExpr(CXXThisExpr* expr)
	{
		QualType pointee = expr->getType()->getPointeeType();
		if(getSemantic(pointee) == Semantic::Value)
			out() << "(&this)[0..1]";
		else
			out() << "this";
		return true;
	}

	bool isStdArray(QualType const& type)
	{
		QualType const rawType = type.isCanonical() ?
		                         type :
		                         type.getCanonicalType();
		std::string const name = rawType.getAsString();
		static std::string const boost_array = "class boost::array<";
		static std::string const std_array = "class std::array<";
		return
		  name.substr(0, boost_array.size()) == boost_array ||
		  name.substr(0, std_array.size()) == std_array;
	}

	bool isStdUnorderedMap(QualType const& type)
	{
		QualType const rawType = type.isCanonical() ?
		                         type :
		                         type.getCanonicalType();
		std::string const name = rawType.getAsString();
		static std::string const std_unordered_map = "class std::unordered_map<";
		return name.substr(0, std_unordered_map.size()) == std_unordered_map;
	}

	bool TraverseCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr* expr)
	{
		return TraverseMemberExprImpl(expr);
	}

	bool TraverseMemberExpr(MemberExpr* Stmt)
	{
		return TraverseMemberExprImpl(Stmt);
	}

	template<typename ME>
	bool TraverseMemberExprImpl(ME* Stmt)
	{
		if(Stmt->getQualifier())
			TraverseNestedNameSpecifier(Stmt->getQualifier());
		DeclarationName const declName = Stmt->getMemberNameInfo().getName();
		std::string const memberName = Stmt->getMemberNameInfo().getAsString();
		Expr* base = Stmt->isImplicitAccess() ? nullptr : Stmt->getBase();
		if(base && isStdArray(base->getType()) && memberName == "assign")
		{
			TraverseStmt(base);
			out() << "[] = ";
			return true;
		}
		if(base && base->getStmtClass() != Stmt::StmtClass::CXXThisExprClass)
		{
			TraverseStmt(base);
			out() << '.';
		}
		auto const kind = declName.getNameKind();
		if(kind == DeclarationName::NameKind::CXXConversionFunctionName)
		{
			out() << "opCast!(";
			PrintType(declName.getCXXNameType());
			out() << ')';
		}
		else
			out() << memberName;
		auto TAL = Stmt->getTemplateArgs();
		auto const tmpArgCount = Stmt->getNumTemplateArgs();
		Spliter spliter(", ");
		if(tmpArgCount != 0)
		{
			pushStream();
			for(unsigned I = 0; I < tmpArgCount; ++I)
			{
				spliter.split();
				PrintTemplateArgument(TAL[I].getArgument());
			}
			printTmpArgList(popStream());
		}

		return true;
	}

	bool TraverseCXXMemberCallExpr(CXXMemberCallExpr* Stmt)
	{
		TraverseStmt(Stmt->getCallee());
		out() << '(';
		Spliter spliter(", ");
		for(auto c : Stmt->arguments())
		{
			if(c->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(c);
			}
		}
		out() << ')';
		return true;
	}

	bool TraverseCXXStaticCastExpr(CXXStaticCastExpr* Stmt)
	{
		out() << "cast(";
		PrintType(Stmt->getTypeInfoAsWritten()->getType());
		out() << ')';
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseCStyleCastExpr(CStyleCastExpr* Stmt)
	{
		out() << "cast(";
		PrintType(Stmt->getTypeInfoAsWritten()->getType());
		out() << ')';
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseConditionalOperator(ConditionalOperator* op)
	{
		TraverseStmt(op->getCond());
		out() << "? ";
		TraverseStmt(op->getTrueExpr());
		out() << ": ";
		TraverseStmt(op->getFalseExpr());
		return true;
	}

	bool TraverseCompoundAssignOperator(CompoundAssignOperator* op)
	{
		VisitorToD::TraverseBinaryOperator(op);
		return true;
	}

	bool TraverseBinAddAssign(CompoundAssignOperator* expr)
	{
		if(expr->getLHS()->getType()->isPointerType())
		{
			TraverseStmt(expr->getLHS());
			out() << ".popFrontN(";
			TraverseStmt(expr->getRHS());
			out() << ')';
			extern_includes.insert("std.range.primitives");
			return true;
		}
		else
			return TraverseCompoundAssignOperator(expr);
	}


#define OPERATOR(NAME)                                        \
	bool TraverseBin##NAME##Assign(CompoundAssignOperator *S) \
	{return TraverseCompoundAssignOperator(S);}
	OPERATOR(Mul) OPERATOR(Div) OPERATOR(Rem) OPERATOR(Sub)
	OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or) OPERATOR(Xor)
#undef OPERATOR


	bool TraverseSubstNonTypeTemplateParmExpr(SubstNonTypeTemplateParmExpr* Expr)
	{
		TraverseStmt(Expr->getReplacement());
		return true;
	}

	bool TraverseBinaryOperator(BinaryOperator* Stmt)
	{
		Expr* lhs = Stmt->getLHS();
		Expr* rhs = Stmt->getRHS();
		Type const* typeL = lhs->getType().getTypePtr();
		Type const* typeR = rhs->getType().getTypePtr();
		if(typeL->isPointerType() and typeR->isPointerType())
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

	bool TraverseBinAdd(BinaryOperator* expr)
	{
		if(expr->getLHS()->getType()->isPointerType())
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
	bool TraverseBin##NAME(BinaryOperator* Stmt) {return TraverseBinaryOperator(Stmt);}
	OPERATOR(PtrMemD) OPERATOR(PtrMemI) OPERATOR(Mul) OPERATOR(Div)
	OPERATOR(Rem) OPERATOR(Sub) OPERATOR(Shl) OPERATOR(Shr)
	OPERATOR(LT) OPERATOR(GT) OPERATOR(LE) OPERATOR(GE) OPERATOR(EQ)
	OPERATOR(NE) OPERATOR(And) OPERATOR(Xor) OPERATOR(Or) OPERATOR(LAnd)
	OPERATOR(LOr) OPERATOR(Assign) OPERATOR(Comma)
#undef OPERATOR

	bool TraverseUnaryOperator(UnaryOperator* Stmt)
	{
		if(Stmt->isIncrementOp())
		{
			if(Stmt->getSubExpr()->getType()->isPointerType())
			{
				TraverseStmt(Stmt->getSubExpr());
				out() << ".popFront";
				extern_includes.insert("std.range.primitives");
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
				if (auto* t = dyn_cast<CXXThisExpr>(Stmt->getSubExpr()))
				{   // (*this) in C++ mean (this) in D
					out() << "this";
					return true;
				}

				preOp = "";
				postOp = "[0]";
			}

			// Avoid to deref struct this
			Expr* expr = static_cast<Expr*>(*Stmt->child_begin());
			bool showOp = true;

			QualType exprType = expr->getType();
			Semantic operSem =
			  exprType->hasPointerRepresentation() ? //expr->getStmtClass() == Stmt::StmtClass::CXXThisExprClass ?
			  getSemantic(exprType->getPointeeType()) :
			  getSemantic(exprType);

			if(operSem == Reference)
			{
				if(Stmt->getOpcode() == UnaryOperatorKind::UO_AddrOf
				   || Stmt->getOpcode() == UnaryOperatorKind::UO_Deref)
					showOp = false;
			}
			if(showOp)
				out() << preOp;
			//TraverseStmt(Stmt->getSubExpr());
			for(auto c : Stmt->children())
				TraverseStmt(c);
			if(showOp)
				out() << postOp;
		}
		return true;
	}
#define OPERATOR(NAME) \
	bool TraverseUnary##NAME(UnaryOperator* Stmt) {return TraverseUnaryOperator(Stmt);}
	OPERATOR(PostInc) OPERATOR(PostDec) OPERATOR(PreInc) OPERATOR(PreDec)
	OPERATOR(AddrOf) OPERATOR(Deref) OPERATOR(Plus) OPERATOR(Minus)
	OPERATOR(Not) OPERATOR(LNot) OPERATOR(Real) OPERATOR(Imag)
	OPERATOR(Extension) OPERATOR(Coawait)
#undef OPERATOR

	template<typename TDeclRefExpr>
	bool TraverseDeclRefExprImpl(TDeclRefExpr* Expr)
	{
		unsigned const argNum = Expr->getNumTemplateArgs();
		if(argNum != 0)
		{
			TemplateArgumentLoc const* tmpArgs = Expr->getTemplateArgs();
			Spliter split(", ");
			pushStream();
			for(unsigned i = 0; i < argNum; ++i)
			{
				split.split();
				PrintTemplateArgument(tmpArgs[i].getArgument());
			}
			printTmpArgList(popStream());
		}

		return true;
	}

	bool TraverseDeclRefExpr(DeclRefExpr* Expr)
	{
		QualType nnsQualType;
		if(Expr->hasQualifier())
		{
			NestedNameSpecifier* nns = Expr->getQualifier();
			if(nns->getKind() == NestedNameSpecifier::SpecifierKind::TypeSpec)
				nnsQualType = nns->getAsType()->getCanonicalTypeUnqualified();
			TraverseNestedNameSpecifier(nns);
		}
		auto decl = Expr->getDecl();
		if(decl->getKind() == Decl::Kind::EnumConstant)
		{
			if(nnsQualType != decl->getType().getUnqualifiedType())
			{
				PrintType(decl->getType());
				out() << '.';
			}
		}
		out() << mangleVar(Expr);
		return TraverseDeclRefExprImpl(Expr);
	}

	bool TraverseDependentScopeDeclRefExpr(DependentScopeDeclRefExpr* expr)
	{
		NestedNameSpecifier* nns = expr->getQualifier();
		TraverseNestedNameSpecifier(nns);
		out() << expr->getDeclName().getAsString();
		return TraverseDeclRefExprImpl(expr);
	}

	bool TraverseRecordType(RecordType* Type)
	{
		out() << mangleType(Type->getDecl());
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
			Spliter spliter2(", ");
			for(unsigned int i = 0, size = tmpArgsSpec.size(); i != size; ++i)
			{
				spliter2.split();
				TemplateArgument const& tmpArg = tmpArgsSpec.get(i);
				PrintTemplateArgument(tmpArg);
			}
			printTmpArgList(popStream());
			break;
		}
		default: assert(false && "Unconsustent RecordDecl kind");
		}
		return true;
	}

	bool TraverseConstantArrayType(ConstantArrayType* Type)
	{
		PrintType(Type->getElementType());
		out() << '[' << Type->getSize().toString(10, false) << ']';
		return true;
	}

	bool TraverseIncompleteArrayType(IncompleteArrayType* Type)
	{
		PrintType(Type->getElementType());
		out() << "[]";
		return true;
	}

	bool TraverseInitListExpr(InitListExpr* Expr)
	{
		bool const isArray =
		  (Expr->ClassifyLValue(*Context) == clang::Expr::LV_ArrayTemporary);
		out() << (isArray ? '[' : '{') << " " << std::endl;
		++indent;
		for(auto c : Expr->inits())
		{
			pushStream();
			TraverseStmt(c);
			std::string const valInit = popStream();
			if(valInit.empty() == false)
				out() << indent_str() << valInit << ',' << std::endl;
			output_enabled = (isInMacro == 0);
		}
		--indent;
		out() << indent_str() << (isArray ? ']' : '}');
		return true;
	}

	bool TraverseParenExpr(ParenExpr* expr)
	{
		if(auto* binOp = dyn_cast<BinaryOperator>(expr->getSubExpr()))
		{
			Expr* lhs = binOp->getLHS();
			Expr* rhs = binOp->getRHS();
			StringLiteral const* strLit = dyn_cast<StringLiteral>(lhs);
			if(strLit && (binOp->getOpcode() == BinaryOperatorKind::BO_Comma))
			{
				StringRef const str = strLit->getString();
				if(str == "__CPP2D__LINE__")
				{
					out() << "__LINE__";
					return true;
				}
				else if(str == "__CPP2D__FILE__")
				{
					out() << "__FILE__ ~ '\\0'";
					return true;
				}
				else if(str == "__CPP2D__FUNC__")
				{
					out() << "__FUNCTION__ ~ '\\0'";
					return true;
				}
				else if(str == "__CPP2D__PFUNC__")
				{
					out() << "__PRETTY_FUNCTION__ ~ '\\0'";
					return true;
				}
				else if(str == "__CPP2D__func__")
				{
					out() << "__PRETTY_FUNCTION__ ~ '\\0'";
					return true;
				}
				else if(str == "CPP2D_MACRO_EXPR")
				{
					auto get_binop = [](Expr * paren)
					{
						return dyn_cast<BinaryOperator>(dyn_cast<ParenExpr>(paren)->getSubExpr());
					};
					BinaryOperator* macro_and_cpp = get_binop(rhs);
					BinaryOperator* macro_name_and_args = get_binop(macro_and_cpp->getLHS());
					auto* macro_name = dyn_cast<StringLiteral>(macro_name_and_args->getLHS());
					auto* macro_args = dyn_cast<CallExpr>(macro_name_and_args->getRHS());
					out() << "(mixin(" << macro_name->getString().str() << "!(";
					PrintMacroArgs(macro_args);
					out() << ")))";
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

	bool TraverseImplicitValueInitExpr(ImplicitValueInitExpr*)
	{
		return true;
	}

	void TraverseVarDeclImpl(VarDecl* Decl)
	{
		std::string const varName = Decl->getNameAsString();
		if(varName.find("CPP2D_MACRO_STMT") == 0)
		{
			PrintStmtMacro(varName, Decl->getInit());
			return;
		}

		if(pass_decl(Decl)) return;

		if(Decl->isOutOfLine())
			return;
		else if(Decl->getOutOfLineDefinition())
			Decl = Decl->getOutOfLineDefinition();

		if(Decl->isStaticDataMember() || Decl->isStaticLocal())
			out() << "static ";
		QualType varType = Decl->getType();
		PrintType(varType);
		out() << " ";
		if(!Decl->isOutOfLine())
		{
			if(auto qualifier = Decl->getQualifier())
				TraverseNestedNameSpecifier(qualifier);
		}
		out() << mangleName(Decl->getNameAsString());
		bool const in_foreach_decl = receiver.forrange_loopvar.count(Decl) != 0;
		if(Decl->hasInit() && !in_foreach_decl)
		{
			Expr* init = Decl->getInit();
			if(Decl->isDirectInit())
			{
				if(auto* constr = dyn_cast<CXXConstructExpr>(init))
				{
					if(getSemantic(varType) == Value)
					{
						if(constr->getNumArgs() != 0)
						{
							out() << " = ";
							PrintCXXConstructExprParams(constr);
						}
					}
					else
					{
						out() << " = new ";
						PrintCXXConstructExprParams(constr);
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
				if(getSemantic(varType) == Value)
				{
					out() << " = ";
					TraverseStmt(init);
				}
				else
				{
					out() << " = new ";
					PrintType(varType);
					out() << '(';
					TraverseStmt(init);
					out() << ')';
				}
			}
		}
	}

	bool TraverseVarDecl(VarDecl* Decl)
	{
		TraverseVarDeclImpl(Decl);
		return true;
	}

	bool VisitDecl(Decl* Decl)
	{
		out() << indent_str() << "/*" << Decl->getDeclKindName() << " Decl*/";
		return true;
	}

	bool VisitStmt(Stmt* Stmt)
	{
		out() << indent_str() << "/*" << Stmt->getStmtClassName() << " Stmt*/";
		return true;
	}

	bool VisitType(Type* Type)
	{
		out() << indent_str() << "/*" << Type->getTypeClassName() << " Type*/";
		return true;
	}

	std::set<std::string> extern_includes;
	std::string modulename;
	std::string insert_after_import;

private:

	const char* getFile(Stmt const* d)
	{
		/*Module* module = d->getLocalOwningModule();
		if (clang::FileEntry const* f = module->getASTFile())
		return f->getName();
		else
		return nullptr;*/
		clang::SourceLocation sl = d->getLocStart();
		if(sl.isValid() == false)
			return "";
		clang::FullSourceLoc fsl = Context->getFullLoc(sl).getExpansionLoc();
		auto& mgr = fsl.getManager();
		if(clang::FileEntry const* f = mgr.getFileEntryForID(fsl.getFileID()))
			return f->getName();
		else
			return nullptr;
	}

	const char* getFile(Decl const* d)
	{
		/*Module* module = d->getLocalOwningModule();
		if (clang::FileEntry const* f = module->getASTFile())
			return f->getName();
		else
			return nullptr;*/
		clang::SourceLocation sl = d->getLocation();
		if(sl.isValid() == false)
			return "";
		clang::FullSourceLoc fsl = Context->getFullLoc(sl).getExpansionLoc();
		auto& mgr = fsl.getManager();
		if(clang::FileEntry const* f = mgr.getFileEntryForID(fsl.getFileID()))
			return f->getName();
		else
			return nullptr;
	}

	bool checkFilename(Decl const* d)
	{
		char const* filepath_str = getFile(d);
		if(filepath_str == nullptr)
			return false;
		std::string filepath = filepath_str;
		if(filepath.size() < 12)
			return false;
		else
		{
			std::string exts[] =
			{
				std::string(".h"),
				std::string(".hpp"),
				std::string(".cpp"),
				std::string(".cxx"),
				std::string(".c"),
			};
			for(auto& ext : exts)
			{
				auto modulenameext = modulename + ext;
				if(modulenameext.size() > filepath.size())
					continue;
				auto filename = filepath.substr(filepath.size() - modulenameext.size());
				if(filename == modulenameext)
					return true;
			}
			return false;
		}
	}

	MatchContainer const& receiver;
	size_t indent = 0;
	ASTContext* Context;
	size_t isInMacro = 0;

	std::vector<std::vector<NamedDecl*> > template_args_stack;
	std::unordered_map<IdentifierInfo*, std::string> renamedIdentifiers;
	std::unordered_map<CXXRecordDecl*, ClassInfo> classInfoMap;
	bool renameIdentifiers = true;
	bool refAccepted = false;
	bool inForRangeInit = false;
};


class VisitorToDConsumer : public clang::ASTConsumer
{
public:
	explicit VisitorToDConsumer(
	  clang::CompilerInstance& Compiler,
	  llvm::StringRef InFile
	)
		: Compiler(Compiler)
		, finder(getMatcher(receiver))
		, finderConsumer(finder.newASTConsumer())
		, InFile(InFile.str())
		, Visitor(&Compiler.getASTContext(), receiver, InFile)
	{
	}

	virtual void HandleTranslationUnit(clang::ASTContext& Context)
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
		if(new_modulename != modulename) // When filename has some illegal characters
			file << "module " << new_modulename << ';';
		for(auto const& import : Visitor.extern_includes)
			file << "import " << import << ";" << std::endl;
		file << "\n\n";
		for(auto const& code : ppcallback.add_before_decl)
			file << code << '\n';
		file << out().str();
	}

private:
	clang::CompilerInstance& Compiler;
	MatchContainer receiver;
	ast_matchers::MatchFinder finder;
	std::unique_ptr<clang::ASTConsumer> finderConsumer;
	std::string InFile;
	VisitorToD Visitor;
};


std::unique_ptr<clang::ASTConsumer> VisitorToDAction::CreateASTConsumer(
  clang::CompilerInstance& Compiler,
  llvm::StringRef InFile
)
{
	auto visitor = std::make_unique<VisitorToDConsumer>(Compiler, InFile);
	return std::move(visitor);
}

bool VisitorToDAction::BeginSourceFileAction(CompilerInstance& ci, StringRef file)
{
	Preprocessor& pp = ci.getPreprocessor();
	std::unique_ptr<Find_Includes> find_includes_callback(new Find_Includes(pp, file));
	pp.addPPCallbacks(std::move(find_includes_callback));
	return true;
}

void VisitorToDAction::EndSourceFileAction()
{
}
