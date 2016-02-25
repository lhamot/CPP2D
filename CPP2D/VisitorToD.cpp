#include "VisitorToD.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <ciso646>

#pragma warning(push, 0)
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/APFloat.h"
#include <clang/AST/Comment.h>
//#include "llvm/Support/ConvertUTF.h"
#pragma warning(pop)

#include "MatchContainer.h"
#include "Matcher.h"

//using namespace clang::tooling;
using namespace llvm;
using namespace clang;

std::vector<std::unique_ptr<std::stringstream> > outStack = []() -> std::vector<std::unique_ptr<std::stringstream> >
{
	std::vector<std::unique_ptr<std::stringstream> > res;
	res.emplace_back(std::make_unique<std::stringstream>());
	return res;
}();

std::stringstream& out()
{
	return *outStack.back();
}

void pushStream()
{
	outStack.emplace_back(std::make_unique<std::stringstream>());
}

std::string popStream()
{
	std::string const str = out().str();
	outStack.pop_back();
	return str;
}

#define CHECK_LOC  if (checkFilename(Decl)) {} else return true

static std::map<std::string, std::string> type2type =
{
	{ "boost::optional", "std.typecons.Nullable" },
	{ "std::vector", "std.container.array.Array" },
	{ "std::set", "std.container.rbtree.RedBlackTree" },
	{ "std::logic_error", "object.error" },
	{ "std::runtime_error", "object.Exception" },
	{ "boost::shared_mutex", "core.sync.rwmutex.ReadWriteMutex" },
	{ "boost::mutex", "core.sync.mutex.Mutex" },
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
	{ "Array", "std.container.array" },
	{ "SafeInt", "std.experimental.safeint.SafeInt" },
	{ "RedBlackTree", "std.container.rbtree" },
	{ "std::map", "cpp_std.map" },
	{ "std::string", "string" },
};


static const std::set<Decl::Kind> noSemiCommaDeclKind =
{
	Decl::Kind::CXXRecord,
	Decl::Kind::Function,
	Decl::Kind::CXXConstructor,
	Decl::Kind::CXXDestructor,
	Decl::Kind::CXXMethod,
	Decl::Kind::Namespace,
	Decl::Kind::NamespaceAlias,
	Decl::Kind::UsingDirective,
	Decl::Kind::Empty,
	Decl::Kind::Friend,
	Decl::Kind::FunctionTemplate,
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
	if (kind == Stmt::StmtClass::DeclStmtClass)
	{
		auto declStmt = static_cast<DeclStmt*>(stmt);
		if (declStmt->isSingleDecl() == false)
			return false;
		else
			return noSemiCommaDeclKind.count(declStmt->getSingleDecl()->getKind()) == 0;
	}
	else
		return noSemiCommaStmtKind.count(kind) == 0;
}

bool needSemiComma(Decl* decl)
{
	if (decl->isImplicit())
		return false;
	auto const kind = decl->getKind();
	if (kind == Decl::Kind::CXXRecord)
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

	Spliter(std::string const& s) :str(s) {}

	void split()
	{
		if (first)
			first = false;
		else
			out() << str;
	}
};

static char const* opStrTab[] =
{
	"@",                ///< Not an overloaded operator
#define OVERLOADED_OPERATOR(Name,Spelling,Token,Unary,Binary,MemberOnly) \
			Spelling,
#include "clang/Basic/OperatorKinds.def"
};

std::string mangleName(std::string const& name)
{
	if (name == "version")
		return "version_";
	if (name == "out")
		return "out_";
	if (name == "in")
		return "in_";
	if (name == "debug")
		return "debug_";
	else if (name == "Exception")
		return "Exception_";
	else
		return name;
}

std::set<std::string> includes_in_file;

class VisitorToD
	: public RecursiveASTVisitor<VisitorToD>
{
	typedef RecursiveASTVisitor<VisitorToD> Base;


	std::string mangleType(NamedDecl const* decl)
	{
		NamedDecl const* can_decl = nullptr;
		std::string const& name = decl->getNameAsString();
		std::string qual_name = name;
		if (Decl const* can_decl_notype = decl->getCanonicalDecl())
		{
			auto const kind = can_decl_notype->getKind();
			if (Decl::Kind::firstNamed <= kind && kind <= Decl::Kind::lastNamed)
			{
				can_decl = static_cast<NamedDecl const*>(can_decl_notype);
				qual_name = can_decl->getQualifiedNameAsString();
			}
		}

		auto qual_type_to_d = type2type.find(qual_name);
		if (qual_type_to_d != type2type.end())
		{
			//There is a convertion to D
			auto const& d_qual_type = qual_type_to_d->second;
			auto const dot_pos = d_qual_type.find_last_of('.');
			auto const module = dot_pos == std::string::npos ?
				std::string() :
				d_qual_type.substr(0, dot_pos);
			if (not module.empty()) //Need an import
				extern_includes.insert(module);
			return d_qual_type.substr(dot_pos + 1);
		}
		else
		{
			std::string decl_inc = getFile(can_decl? can_decl: decl);
			for (std::string include : includes_in_file)
			{
				auto const pos = decl_inc.find(include);
				if (pos != std::string::npos && 
					pos == (decl_inc.size() - include.size()) &&
					(pos == 0 || decl_inc[pos - 1] == '/' || decl_inc[pos - 1] == '\\'))
				{
					if (include.find(".h") == include.size() - 2)
						include = include.substr(0, include.size() - 2);
					if (include.find(".hpp") == include.size() - 4)
						include = include.substr(0, include.size() - 4);
					std::transform(std::begin(include), std::end(include), std::begin(include), tolower);
					std::replace(std::begin(include), std::end(include), '/', '.');
					std::replace(std::begin(include), std::end(include), '\\', '.');
					extern_includes.insert(include);
					break;
				}
			}
			return name;
		}
	}

	std::string replace(std::string str, std::string const& in, std::string const& out)
	{
		size_t pos = 0;
		std::string::iterator iter;
		while ((iter = std::find(std::begin(str) + pos, std::end(str), '\r')) != std::end(str))
		{
			pos = iter - std::begin(str);
			if ((pos + 1) < str.size() && str[pos + 1] == '\n')
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
		if (rc && not rc->isTrailingComment())
		{
			using namespace std;
			out() << std::endl << indent_str();
			string const comment = rc->getRawText(sm).str();
			out() << replace(comment, "\r\n", line_return.str()) << std::endl;
		}
	}

	void printCommentAfter(Decl* t)
	{
		SourceManager& sm = Context->getSourceManager();
		const RawComment* rc = Context->getRawCommentForDeclNoCache(t);
		if (rc && rc->isTrailingComment())
			out() << '\t' << rc->getRawText(sm).str();
	}

	// trim from both ends
	static inline std::string trim(std::string const& s) {
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
			prevIter = iter + 1;
		} while (iter != std::end(instr));
		return result;
	}

	void printStmtComment(SourceLocation& locStart, SourceLocation const& locEnd, SourceLocation const& nextStart = SourceLocation())
	{
		auto& sm = Context->getSourceManager();
		std::string comment = Lexer::getSourceText(CharSourceRange(SourceRange(locStart, locEnd), true), sm, LangOptions()).str();
		std::vector<std::string> comments = split(comment);
		//if (comments.back() == std::string())
		comments.pop_back();
		Spliter split(indent_str());
		if (comments.empty())
			out() << std::endl;
		if(not comments.empty())
		{
			auto& firstComment = comments.front();
			auto commentPos1 = firstComment.find("//");
			auto commentPos2 = firstComment.find("/*");
			size_t trimPos = 0;
			if (commentPos1 != std::string::npos)
			{
				if (commentPos2 != std::string::npos)
					trimPos = std::min(commentPos1, commentPos2);
				else
					trimPos = commentPos1;
			}
			else if (commentPos2 != std::string::npos)
				trimPos = commentPos2;
			else
				firstComment = "";

			if (not firstComment.empty())
				firstComment = firstComment.substr(trimPos);

			out() << " ";
			size_t index = 0;
			for (auto const& c : comments)
			{
				split.split();
				out() << c;
				out() << std::endl;
				++index;
			}
		}
		locStart = nextStart;
	}

public:
	explicit VisitorToD(
			ASTContext *Context, 
			MatchContainer const& receiver)
		: Context(Context)
		, receiver(receiver)
	{
	}

	std::string indent_str() const
	{
		return std::string(indent * 4, ' ');
	}

	bool TraverseTranslationUnitDecl(TranslationUnitDecl *Decl)
	{
		//SourceManager& sm = Context->getSourceManager();
		//SourceLocation prevDeclEnd;
		//for(auto c: Context->Comments.getComments())
		//	out() << c->getRawText(sm).str() << std::endl;

		for (auto c : Decl->decls())
		{
			if (checkFilename(c))
			{
				//SourceLocation newDeclStart = c->getLocStart();
				//CharSourceRange newSourceRange = CharSourceRange::getTokenRange(prevDeclEnd, newDeclStart);
				
				//std::string text = Lexer::getSourceText(newSourceRange, sm, LangOptions(), 0);
				//out() << text;

				/*bool invalid1 = true;
				bool invalid2 = true;
				char const* a = sm.getCharacterData(prevSourceRange.getEnd(), &invalid1);
				char const* b = sm.getCharacterData(newSourceRange.getBegin(), &invalid2);
				if(not invalid1 && not invalid2)
					out() << std::string(a + 3, b);*/

				//prevDeclEnd = c->getLocEnd();
				pushStream();
				TraverseDecl(c);
				std::string const decl = popStream();
				if (not decl.empty())
				{
					printCommentBefore(c);
					out() << indent_str() << decl;
					if (needSemiComma(c))
						out() << ';';
					printCommentAfter(c);
					out() << std::endl << std::endl;
				}
			}
		}

		return true;
	}

	bool TraverseTypedefDecl(TypedefDecl *Decl)
	{
		out() << "alias " << mangleName(Decl->getNameAsString()) << " = ";
		PrintType(Decl->getUnderlyingType());
		return true;
	}

	bool TraverseFieldDecl(FieldDecl *Decl)
	{
		//out() << indent_str();
		//TraverseNestedNameSpecifierLoc(Decl->getQualifierLoc());
		PrintType(Decl->getType());
		out() << " " << mangleName(Decl->getNameAsString());
		if (Decl->hasInClassInitializer())
		{
			out() << " = ";
			TraverseStmt(Decl->getInClassInitializer());
		}
		//out() << ";" << std::endl;
		return true;
	}

	bool TraverseElaboratedType(ElaboratedType *Type)
	{
		if(Type->getQualifier())
			TraverseNestedNameSpecifier(Type->getQualifier());
		PrintType(Type->getNamedType());
		return true;
	}

	bool TraverseSubstTemplateTypeParmType(SubstTemplateTypeParmType*)
	{
		return true;
	}

	bool TraverseNestedNameSpecifier(NestedNameSpecifier *NNS)
	{
		auto kind = NNS->getKind();
		if (kind == NestedNameSpecifier::TypeSpec || 
			kind == NestedNameSpecifier::TypeSpecWithTemplate ||
			kind == NestedNameSpecifier::Identifier
			)
		{
			PrintType(QualType(NNS->getAsType(), 0));
			out() << ".";
		}
		return true;
	}

	bool tempArgHasTempArg(TemplateArgument const& type)
	{
		switch (type.getKind())
		{
		case TemplateArgument::ArgKind::Null:
			return false;
		case TemplateArgument::ArgKind::Type:
		{
			QualType qual = type.getAsType();
			auto kind = qual.getTypePtr()->getTypeClass();
			return kind == Type::TypeClass::TemplateSpecialization
				|| kind == Type::TypeClass::Elaborated;
		}
		case TemplateArgument::ArgKind::Declaration:
			return true;
		case TemplateArgument::ArgKind::NullPtr:
			return false;
		case TemplateArgument::ArgKind::Integral:
			return false;
		case TemplateArgument::ArgKind::Template:
			return true;
		case TemplateArgument::ArgKind::TemplateExpansion:
			return true;
		case TemplateArgument::ArgKind::Expression:
		{
			Expr* expr = type.getAsExpr();
			return expr->isTypeDependent() || expr->isValueDependent();
		}
		case TemplateArgument::ArgKind::Pack:
			return true;
		}
		return true;
	}

	bool TraverseTemplateSpecializationType(TemplateSpecializationType *Type)
	{
		if(isStdArray(Type->desugar()))
		{
			TraverseTemplateArgument(Type->getArg(0));
			out() << '[';
			TraverseTemplateArgument(Type->getArg(1));
			out() << ']';
			return true;
		}
		else if (isStdUnorderedMap(Type->desugar()))
		{
			TraverseTemplateArgument(Type->getArg(1));
			out() << '[';
			TraverseTemplateArgument(Type->getArg(0));
			out() << ']';
			return true;
		}
		out() << mangleType(Type->getTemplateName().getAsTemplateDecl());
		auto const argNum = Type->getNumArgs();
		bool const needParen = (argNum > 1) || (argNum == 1 && tempArgHasTempArg(Type->getArg(0)));
		out() << (needParen ? "!(": "!");
		Spliter spliter(", ");
		for (unsigned int i = 0; i < argNum; ++i)
		{
			spliter.split();
			TraverseTemplateArgument(Type->getArg(i));
		}
		if(needParen)
			out() << ")";
		return true;
	}

	bool TraverseTypedefType(TypedefType* Type)
	{
		std::string const name = mangleType(Type->getDecl());
		std::string const converted = [&]() -> std::string
		{
			return
				/*name == "int8_t"	? std::string("byte") :
				name == "uint8_t"	? std::string("ubyte") :
				name == "int16_t"	? std::string("short") :
				name == "uint16_t"	? std::string("ushort") :
				name == "int32_t"	? std::string("int") :
				name == "uint32_t"	? std::string("uint") :
				name == "int64_t"	? std::string("long") :
				name == "uint64_t"	? std::string("ulong") :*/
				name;
		}();
		out() << converted;
		return true;
	}

	template<typename InitList>
	bool TraverseCompoundStmtImpl(CompoundStmt *Stmt, InitList initList)
	{
		SourceLocation locStart = Stmt->getLBracLoc().getLocWithOffset(1);
		out() << "{";
		++indent;
		initList();
		for (auto child : Stmt->children())
		{
			printStmtComment(locStart, child->getLocStart().getLocWithOffset(-1), child->getLocEnd());
			out() << indent_str();
			TraverseStmt(child);
			if (needSemiComma(child))
				out() << ";";
		}
		printStmtComment(locStart, Stmt->getRBracLoc().getLocWithOffset(-1));
		--indent;
		out() << indent_str();
		out() << "}";
		return true;
	}

	bool TraverseCompoundStmt(CompoundStmt *Stmt)
	{
		return TraverseCompoundStmtImpl(Stmt, []{});
	}

	template<typename InitList>
	bool TraverseCXXTryStmtImpl(CXXTryStmt *Stmt, InitList initList)
	{
		out() << "try" << std::endl << indent_str();
		auto tryBlock = Stmt->getTryBlock();
		TraverseCompoundStmtImpl(tryBlock, initList);
		auto handlerCount = Stmt->getNumHandlers();
		for (decltype(handlerCount) i = 0; i < handlerCount; ++i)
		{
			out() << std::endl << indent_str();
			TraverseStmt(Stmt->getHandler(i));
		}
		return true;
	}

	bool TraverseCXXTryStmt(CXXTryStmt *Stmt)
	{
		return TraverseCXXTryStmtImpl(Stmt, [] {});
	}

	bool TraverseNamespaceDecl(NamespaceDecl *Decl)
	{
		out() << "// -> module " << mangleName(Decl->getNameAsString()) << ';' << std::endl;
		for (auto decl : Decl->decls())
		{
			pushStream();
			TraverseDecl(decl);
			std::string const declstr = popStream();
			if (not declstr.empty())
			{
				printCommentBefore(decl);
				out() << indent_str() << declstr;
				if (needSemiComma(decl))
					out() << ';';
				printCommentAfter(decl);
				out() << std::endl << std::endl;
			}
		}
		out() << "// <- module " << mangleName(Decl->getNameAsString()) << " end" << std::endl << std::flush;
		return true;
	}

	bool TraverseCXXCatchStmt(CXXCatchStmt* Stmt)
	{
		out() << "catch";
		if (Stmt->getExceptionDecl())
		{
			out() << '(';
			isCatchParameter = true;
			TraverseVarDeclImpl(Stmt->getExceptionDecl());
			isCatchParameter = false;
			out() << ')';
		}
		out() << std::endl;
		out() << indent_str();
		TraverseStmt(Stmt->getHandlerBlock());
		return true;
	}

	bool TraverseAccessSpecDecl(AccessSpecDecl*)
	{
		return true;
	}

	bool TraverseCXXRecordDecl(CXXRecordDecl *Decl)
	{
		return TraverseCXXRecordDeclImpl(Decl, [] {});
	}

	bool TraverseRecordDecl(RecordDecl *Decl)
	{
		return TraverseCXXRecordDeclImpl(Decl, [] {});
	}

	template<typename TmpSpecFunc>
	bool TraverseCXXRecordDeclImpl(
		RecordDecl *Decl, 
		TmpSpecFunc traverseTmpSpecs)
	{
		if (Decl->isCompleteDefinition() == false)
			return true;

		const bool isClass = Decl->isClass();// || Decl->isPolymorphic();
		char const* struct_class = Decl->isClass() ? "class" : "struct";
		out() << struct_class << " " << mangleName(Decl->getNameAsString());
		traverseTmpSpecs();

		out() << std::endl << indent_str() << "{";
		++indent;

		static char const* AccessSpecifierStr[] =
		{
			"public",
			"protected",
			"private"
		};

		//Base::TraverseCXXRecordDecl(Decl);
		AccessSpecifier access = isClass ?
			AccessSpecifier::AS_private :
			AccessSpecifier::AS_public;
		for (auto decl : Decl->decls())
		{
			pushStream();
			TraverseDecl(decl);
			std::string const declstr = popStream();
			if (not declstr.empty())
			{
				AccessSpecifier newAccess = decl->getAccess();
				if (newAccess == AccessSpecifier::AS_none)
					newAccess = AccessSpecifier::AS_public;
				if (newAccess != access)
				{
					--indent;
					out() << indent_str() << AccessSpecifierStr[newAccess] << ":" << std::endl;
					++indent;
					access = newAccess;
				}
				printCommentBefore(decl);
				out() << indent_str() << declstr;
				if (needSemiComma(decl))
					out() << ";";
				printCommentAfter(decl);
				out() << std::endl;
			}
		}
		--indent;
		out() << indent_str() << "}";

		return true;
	}

	bool TraverseClassTemplateDecl(ClassTemplateDecl* Decl)
	{
		TraverseCXXRecordDeclImpl(Decl->getTemplatedDecl(), [Decl, this]
		{
			auto& tmpParams = *Decl->getTemplateParameters();
			out() << "(";
			Spliter spliter1(", ");
			for (decltype(tmpParams.size()) i = 0, size = tmpParams.size(); i != size; ++i)
			{
				spliter1.split();
				TraverseDecl(tmpParams.getParam(i));
			}
			out() << ')';
		});
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

	bool TraverseClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl* Decl)
	{
		return TraverseClassTemplateSpecializationDeclImpl(Decl);
	}

	bool TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* Decl)
	{
		return TraverseClassTemplateSpecializationDeclImpl(Decl);
	}

	template<typename D>
	bool TraverseClassTemplateSpecializationDeclImpl(D* Decl)
	{
		if (Decl->isCompleteDefinition() == false)
			return true;

		template_args_stack.emplace_back();
		auto* tmpParams = getTemplateParameters(Decl);
		if (tmpParams)
		{
			auto& template_args = template_args_stack.back();
			for (decltype(tmpParams->size()) i = 0, size = tmpParams->size(); i != size; ++i)
				template_args.push_back(tmpParams->getParam(i));
		}

		TraverseCXXRecordDeclImpl(Decl, [Decl, this, tmpParams]
		{
			auto& specializedTmpParams = *Decl->getSpecializedTemplate()->getTemplateParameters();
			auto& tmpArgs = Decl->getTemplateArgs();
			assert(tmpArgs.size() == specializedTmpParams.size());
			out() << '(';
			Spliter spliter2(", ");
			for (decltype(tmpArgs.size()) i = 0, size = tmpArgs.size(); i != size; ++i)
			{
				spliter2.split();
				TraverseDecl(specializedTmpParams.getParam(i));
				out() << " : ";
				TraverseTemplateArgument(tmpArgs.get(i));
			}
			if (tmpParams)
			{
				for (decltype(tmpParams->size()) i = 0, size = tmpParams->size(); i != size; ++i)
				{
					spliter2.split();
					TraverseDecl(tmpParams->getParam(i));
				}
			}
			out() << ')';
		});
		template_args_stack.pop_back();
		return true;
	}

	bool TraverseCXXConstructorDecl(CXXConstructorDecl *Decl)
	{
		return TraverseFunctionDeclImpl(Decl, [] {});
	}

	bool TraverseCXXDestructorDecl(CXXDestructorDecl *Decl)
	{
		return TraverseFunctionDeclImpl(Decl, [] {});
	}

	bool TraverseCXXMethodDecl(CXXMethodDecl *Decl)
	{
		if (Decl->getLexicalParent() == Decl->getParent())
			return TraverseFunctionDeclImpl(Decl, [] {});
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
	
	bool TraverseCXXUnresolvedConstructExpr(CXXUnresolvedConstructExpr  *Expr)
	{
		PrintType(Expr->getTypeAsWritten());
		Spliter spliter(", ");
		out() << "(";
		for (decltype(Expr->arg_size()) i = 0; i < Expr->arg_size(); ++i)
		{
			auto arg = Expr->getArg(i);
			if (arg->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(arg);
			}
		}
		out() << ")";
		return true;
	}

	bool TraverseUnresolvedLookupExpr(UnresolvedLookupExpr  *Expr)
	{
		out() << mangleName(Expr->getName().getAsString());
		if (Expr->hasExplicitTemplateArgs())
		{
			auto const argNum = Expr->getNumTemplateArgs();
			bool const needParen = (argNum > 1) || (argNum == 1 && tempArgHasTempArg(Expr->getTemplateArgs()[0].getArgument()));
			out() << (needParen? "!(" : "!");
			Spliter spliter(", ");
			for (size_t i = 0; i < argNum; ++i)
			{
				spliter.split();
				auto tmpArg = Expr->getTemplateArgs()[i];
				TraverseTemplateArgumentLoc(tmpArg);
			}
			out() << (needParen ? ")" : "");
		}
		return true;
	}

	bool TraverseCXXForRangeStmt(CXXForRangeStmt  *Stmt)
	{
		out() << "foreach(";
		DeclStmt* varDecl = Stmt->getLoopVarStmt();
		if(varDecl->isSingleDecl() == false)
			abort();
		Decl* singleDecl = varDecl->getSingleDecl();
		if(singleDecl->getKind() != Decl::Kind::Var)
			abort();
		TraverseVarDeclImpl(static_cast<VarDecl*>(singleDecl));
		out() << "; ";
		TraverseStmt(Stmt->getRangeInit());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		return true;
	}

	bool TraverseDoStmt(DoStmt  *Stmt)
	{
		out() << "do" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getBody());
		out() << "while(";
		TraverseStmt(Stmt->getCond());
		out() << ")";
		return true;
	}

	bool TraverseSwitchStmt(SwitchStmt *Stmt)
	{
		out() << "switch(";
		TraverseStmt(Stmt->getCond());
		out() << ")" << std::endl << indent_str();
		TraverseStmt(Stmt->getBody());
		return true;
	}

	bool TraverseCaseStmt(CaseStmt *Stmt)
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

	bool TraverseStaticAssertDecl(StaticAssertDecl *Decl)
	{
		out() << "static assert(";
		TraverseStmt(Decl->getAssertExpr());
		out() << ", ";
		TraverseStmt(Decl->getMessage());
		out() << ")";
		return true;
	}

	bool TraverseDefaultStmt(DefaultStmt *Stmt)
	{
		out() << "default:" << std::endl;
		++indent;
		out() << indent_str();
		TraverseStmt(Stmt->getSubStmt());
		--indent;
		return true;
	}

	bool TraverseCXXNewExpr(CXXNewExpr *Expr)
	{
		out() << "new ";
		TraverseStmt(Expr->getInitializer());
		return true;
	}

	void PrintCXXConstructExprParams(CXXConstructExpr *Init)
	{
		PrintType(Init->getType());
		out() << '(';
		Spliter spliter(", ");
		for (auto arg : Init->arguments())
		{
			if (arg->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
			{
				spliter.split();
				TraverseStmt(arg);
			}
		}
		out() << ')';
	}

	bool TraverseCXXConstructExpr(CXXConstructExpr *Init)
	{
		//Can't put the " = " here can called by ReturnStmt
		if (Init->isElidable())  // Elidable ?
		{
			//out() << "/*el*/";
			assert(Init->getNumArgs() == 0);
			TraverseStmt(*Init->arg_begin());
			return true;
		}
		else if (Init->getConstructor()->isExplicit() == false && Init->getNumArgs() == 1)
		{
			//out() << "/*im*/";
			TraverseStmt(*Init->arg_begin());  // Implicite convertion is enough
		}
		else
			PrintCXXConstructExprParams(Init);
		return true;
	}

	void PrintType(QualType const& type)
	{
		if (type.getTypePtr()->getTypeClass() == Type::TypeClass::Auto)
		{
			if (type.isConstQualified())
				out() << "const ";
			TraverseType(type);
		}
		else
		{
			if (type.isConstQualified())
				out() << "const(";
			TraverseType(type);
			if (type.isConstQualified())
				out() << ')';
		}
	}

	bool TraverseConstructorInitializer(CXXCtorInitializer *Init)
	{
		if(Init->getMember())
			out() << mangleName(Init->getMember()->getNameAsString());
		if (TypeSourceInfo *TInfo = Init->getTypeSourceInfo())
			PrintType(TInfo->getType());
		out() << " = ";
		TraverseStmt(Init->getInit());

		if (Init->getNumArrayIndices() && getDerived().shouldVisitImplicitCode())
			for (VarDecl *VD : Init->getArrayIndexes())
				TraverseDecl(VD);
		return true;
	}


	void startCtorBody(FunctionDecl*){}

	void startCtorBody(CXXConstructorDecl *Decl)
	{
		auto ctor_init_count = Decl->getNumCtorInitializers();
		if (ctor_init_count != 0)
		{
			for (auto init : Decl->inits())
			{
				if (init->isWritten())
				{
					out() << std::endl << indent_str();
					TraverseConstructorInitializer(init);
					out() << ";";
				}
			}
		}
	}

	bool printFuncBegin(FunctionDecl* Decl)
	{
		if (Decl->isOverloadedOperator() && Decl->getOverloadedOperator() == OverloadedOperatorKind::OO_ExclaimEqual)
			return false;
		PrintType(Decl->getReturnType());
		out() << " ";
		if (Decl->isOverloadedOperator())
		{
			auto const opKind = Decl->getOverloadedOperator();
			if (opKind == OverloadedOperatorKind::OO_EqualEqual)
				out() << "opEqual";
			else
				out() << "opBinary(string op: \"" << opStrTab[opKind] << "\")";
		}
		else
			out() << mangleName(Decl->getNameAsString());
		return true;
	}

	bool printFuncBegin(CXXConstructorDecl *Decl)
	{
		auto record = Decl->getParent();
		if (record->isStruct() && Decl->getNumParams() == 0)
			return false;
		out() << "this";
		return true;
	}

	bool printFuncBegin(CXXDestructorDecl*)
	{
		out() << "~this";
		return true;
	}


	template<typename D, typename TemplPrinter>
	bool TraverseFunctionDeclImpl(D *Decl, TemplPrinter templPrinter = [] {})
	{
		if (printFuncBegin(Decl) == false)
			return true;
		templPrinter();
		out() << "(";
		TypeSourceInfo* declSourceInfo = Decl->getTypeSourceInfo();
		TypeLoc declTypeLoc = declSourceInfo->getTypeLoc();
		FunctionTypeLoc funcTypeLoc = declTypeLoc.castAs<FunctionTypeLoc>();
		SourceLocation locStart = funcTypeLoc.getLParenLoc().getLocWithOffset(1);
		++indent;
		size_t index = 0;
		for (auto decl : Decl->params())
		{
			printStmtComment(locStart, decl->getLocStart().getLocWithOffset(-1), decl->getLocEnd().getLocWithOffset(1));
			out() << indent_str();
			TraverseDecl(decl);
			if(index != Decl->getNumParams() - 1)
				out() << ',';
			++index;
		}
		printStmtComment(locStart, funcTypeLoc.getRParenLoc());
		--indent;
		out() << indent_str();
		out() << ")";
		Stmt* body = Decl->getBody();
		if(body)
		{
			out() << std::endl << std::flush;
			if (body->getStmtClass() == Stmt::CXXTryStmtClass)
			{
				out() << indent_str() << '{' << std::endl;
				++indent;
				out() << indent_str();
				TraverseCXXTryStmtImpl(static_cast<CXXTryStmt*>(body), [&] {startCtorBody(Decl); });
				out() << std::endl;
				--indent;
				out() << indent_str() << '}';
			}
			else
			{
				out() << indent_str();
				assert(body->getStmtClass() == Stmt::CompoundStmt);
				TraverseCompoundStmtImpl(static_cast<CompoundStmt*>(body), [&] {startCtorBody(Decl); });
			}
		}
		else
			out() << ";";
		return true;

	}

	bool TraverseFunctionDecl(FunctionDecl *Decl)
	{
		return TraverseFunctionDeclImpl(Decl, [] {});
	}

	bool TraverseUsingDirectiveDecl(UsingDirectiveDecl*)
	{
		return true;
	}


	bool TraverseFunctionTemplateDecl(FunctionTemplateDecl *Decl)
	{
		auto FDecl = Decl->getTemplatedDecl();
		return TraverseFunctionDeclImpl(FDecl, [FDecl, Decl, this] {
			out() << "(";
			Spliter spliter(", ");
			for (TemplateArgument const& tmpArg : Decl->getInjectedTemplateArgs())
			{
				spliter.split();
				PrintType(tmpArg.getAsType());
			}
			out() << ")";
		});
	}

	bool TraverseBuiltinType(BuiltinType *Type)
	{
		//PrintingPolicy pp = LangOptions();
		//pp.Bool = 1;
		//out() << Type->getNameAsCString(pp);
		out() << [Type]
		{
			switch (Type->getKind())
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
			}
		}();
		return true;
	}

	bool TraversePointerType(PointerType *Type)
	{
		auto const pointee = Type->getPointeeType();
		if (pointee->getTypeClass() == Type::Paren) //function pointer do not need '*'
		{
			auto innerType = static_cast<ParenType const*>(pointee.getTypePtr())->getInnerType();
			if (innerType->getTypeClass() == Type::FunctionProto)
				return TraverseType(innerType);
		}
		PrintType(pointee);
		out() << '*';
		return true;
	}

	bool TraverseCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr*)
	{
		out() << "null";
		return true;
	}

	bool TraverseEnumConstantDecl(EnumConstantDecl *Decl)
	{
		out() << mangleName(Decl->getNameAsString());
		if (Decl->getInitExpr())
		{
			out() << " = ";
			TraverseStmt(Decl->getInitExpr());
		}
		return true;
	}

	bool TraverseEnumDecl(EnumDecl *Decl)
	{
		out() << "enum " << mangleName(Decl->getNameAsString());
		if (Decl->isFixed())
		{
			out() << " : ";
			TraverseType(Decl->getIntegerType());
		}
		out() << std::endl << indent_str() << "{" << std::endl;
		++indent;
		for (auto e : Decl->enumerators())
		{
			out() << indent_str();
			TraverseDecl(e);
			out() << "," << std::endl;
		}
		--indent;
		out() << indent_str() << "}";
		return true;
	}

	bool TraverseEnumType(EnumType *Type)
	{
		out() << mangleName(Type->getDecl()->getNameAsString());
		return true;
	}

	bool TraverseIntegerLiteral(IntegerLiteral *Stmt)
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

	bool TraverseAutoType(AutoType* type)
	{
		if(receiver.forrange_loopvar_auto.count(type) == 0)
			out() << "auto";
		return true;
	}

	/*bool TraverseMemberPointerType(MemberPointerType *Type)
	{
	//out() << Type->getTypeClassName();
	return true;
	}*/
	
	bool TraverseLinkageSpecDecl(LinkageSpecDecl*)
	{
		return true;
	}

	bool TraverseFriendDecl(FriendDecl*)
	{
		return true;
	}

	bool TraverseParmVarDecl(ParmVarDecl *Decl)
	{
		PrintType(Decl->getType());
		out() <<  " " << mangleName(Decl->getNameAsString());
		return true;
	}

	bool TraverseRValueReferenceType(RValueReferenceType *Type)
	{
		PrintType(Type->getPointeeType());
		out() << "&&";
		return true;
	}

	bool TraverseLValueReferenceType(LValueReferenceType *Type)
	{
		if(not isCatchParameter)
			out() << "ref ";
		PrintType(Type->getPointeeType());
		return true;
	}

	bool TraverseTemplateTypeParmType(TemplateTypeParmType *Type)
	{
		//Type->dump();
		IdentifierInfo* identifier = Type->getIdentifier();
		if (identifier)
			out() << identifier->getName().str();
		else if (Type->getDecl())
			TraverseDecl(Type->getDecl());
		else
		{
			auto param = template_args_stack[Type->getDepth()][Type->getIndex()];
			TraverseDecl(param);
			//out() << "type-parameter-" << Type->getDepth() << '-' << Type->getIndex();
		}
		return true;
	}

	bool TraverseTemplateTypeParmDecl(TemplateTypeParmDecl *Decl)
	{
		IdentifierInfo* identifier = Decl->getIdentifier();
		if (identifier)
			out() << identifier->getName().str();
		return true;
	}

	bool TraverseNonTypeTemplateParmDecl(NonTypeTemplateParmDecl *Decl)
	{
		IdentifierInfo* identifier = Decl->getIdentifier();
		if (identifier)
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
			for (auto d : Stmt->decls())
			{
				printCommentBefore(d);
				if (first)
					first = false;
				else
					out() << indent_str();
				TraverseDecl(d);
				out() << ";";
				printCommentAfter(d);
				out() << std::endl;
			}
		}
		return true;
	}

	bool TraverseNamespaceAliasDecl(NamespaceAliasDecl*)
	{
		return true;
	}

	bool TraverseReturnStmt(ReturnStmt* Stmt)
	{
		out() << "return";
		if (Stmt->getRetValue())
		{
			out() << ' ';
			TraverseStmt(Stmt->getRetValue());
		}
		return true;
	}

	bool TraverseCXXOperatorCallExpr(CXXOperatorCallExpr* Stmt)
	{
		const OverloadedOperatorKind kind = Stmt->getOperator();
		if (kind == OverloadedOperatorKind::OO_Call || kind == OverloadedOperatorKind::OO_Subscript)
		{
			auto opStr = opStrTab[kind];
			auto iter = Stmt->arg_begin(), end = Stmt->arg_end();
			TraverseStmt(*iter);
			Spliter spliter(", ");
			out() << opStr[0];
			for (++iter; iter != end; ++iter)
			{
				if ((*iter)->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
				{
					spliter.split();
					TraverseStmt(*iter);
				}
			}
			out() << opStr[1];
		}
		else if (kind == OverloadedOperatorKind::OO_Arrow)
		{
			TraverseStmt(*Stmt->arg_begin());
		}
		else
		{
			auto const numArgs = Stmt->getNumArgs();
			if (numArgs == 2)
			{
				TraverseStmt(*Stmt->arg_begin());
				out() << " ";
			}
			assert(kind < 1 && kind < (sizeof(opStrTab) / sizeof(char const*)));
			out() << opStrTab[kind];
			if (numArgs == 2)
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
		if (Stmt->getStmtClass() == Stmt::StmtClass::CompoundStmtClass)
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
		Expr->getValue().toString(str);
		out() << str.c_str();
		if (APFloat::semanticsSizeInBits(sem) < 64)
			out() << 'f';
		else if (APFloat::semanticsSizeInBits(sem) > 64)
			out() << 'l';
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

	bool TraverseIfStmt(IfStmt* Stmt)
	{
		out() << "if(";
		TraverseStmt(Stmt->getCond());
		out() << ")" << std::endl;
		TraverseCompoundStmtOrNot(Stmt->getThen());
		if(Stmt->getElse())
		{
			out() << std::endl << indent_str() << "else ";
			if (Stmt->getElse()->getStmtClass() == Stmt::IfStmtClass)
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
		out() << '(';
		PrintType(Type->getInnerType());
		out() << ')';
		return true;
	}

	bool TraverseFunctionProtoType(FunctionProtoType* Type)
	{
		PrintType(Type->getReturnType());
		out() << " function(";
		Spliter spliter(", ");
		for (auto const& p : Type->getParamTypes())
		{
			spliter.split();
			PrintType(p);
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

	bool TraverseStringLiteral(StringLiteral* Stmt)
	{
		out() << "\"";
		std::string literal;
		auto str = Stmt->getString();
		if (Stmt->isUTF16() || Stmt->isWide())
		{
			static_assert(sizeof(unsigned short) == 2, "sizeof(unsigned short) == 2 expected");
			std::basic_string<unsigned short> literal16((unsigned short*)str.data(), str.size() / 2);
			std::wstring_convert<std::codecvt_utf8<unsigned short>, unsigned short> cv;
			literal = cv.to_bytes(literal16);
		}
		else if (Stmt->isUTF32())
		{
			static_assert(sizeof(unsigned int) == 4, "sizeof(unsigned int) == 4 required");
			std::basic_string<unsigned int> literal32((unsigned int*)str.data(), str.size() / 4);
			std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> cv;
			literal = cv.to_bytes(literal32);
		}
		else
			literal = std::string(str.data(), str.size());
		size_t pos = 0;
		while ((pos = literal.find('\\', pos)) != std::string::npos)
		{
			literal = literal.substr(0, pos) + "\\\\" + literal.substr(pos + 1);
			pos += 2;
		}
		pos = std::string::npos;
		while ((pos = literal.find('\n')) != std::string::npos)
			literal = literal.substr(0, pos) + "\\n" + literal.substr(pos + 1);
		pos = 0;
		while ((pos = literal.find('"', pos)) != std::string::npos)
		{
			if (pos > 0 && literal[pos - 1] == '\\')
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
		switch (Expr->getKind())
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

	bool TraverseEmptyDecl(EmptyDecl*)
	{
		return true;
	}

	
	bool TraverseLambdaExpr(LambdaExpr* S)
	{
		TypeLoc TL = S->getCallOperator()->getTypeSourceInfo()->getTypeLoc();
		FunctionProtoTypeLoc Proto = TL.castAs<FunctionProtoTypeLoc>();

		if (S->hasExplicitParameters() && S->hasExplicitResultType()) 
		{
			out() << '(';
			// Visit the whole type.
			TraverseTypeLoc(TL);
			out() << ')' << std::endl;
		}
		else 
		{
			if (S->hasExplicitParameters()) {
				Spliter spliter2(", ");
				out() << '(';
				// Visit parameters.
				for (unsigned I = 0, N = Proto.getNumParams(); I != N; ++I) 
				{
					spliter2.split();
					TraverseDecl(Proto.getParam(I));
				}
				out() << ')' << std::endl;
			}
			else if (S->hasExplicitResultType()) {
				TraverseTypeLoc(Proto.getReturnLoc());
			}

			auto *T = Proto.getTypePtr();
			for (const auto &E : T->exceptions()) {
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

	bool TraverseCallExpr(CallExpr* Stmt)
	{
		//out() << indent_str();
		TraverseStmt(Stmt->getCallee());
		out() << "(";
		Spliter spliter(", ");
		for (auto c : Stmt->arguments())
		{
			if (c->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
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
		TraverseStmt(Stmt->getSubExpr());
		return true;
	}

	bool TraverseCXXThisExpr(CXXThisExpr*)
	{
		out() << "this";
		return true;
	}

	bool isStdArray(QualType const& type)
	{
		QualType const rawType = type.isCanonical()?
			type:
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

	bool TraverseCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr* Stmt)
	{
		return TraverseMemberExprImpl(Stmt);
	}

	bool TraverseMemberExpr(MemberExpr* Stmt)
	{
		return TraverseMemberExprImpl(Stmt);
	}

	template<typename ME>
	bool TraverseMemberExprImpl(ME* Stmt)
	{
		DeclarationName const declName = Stmt->getMemberNameInfo().getName();
		std::string const memberName = Stmt->getMemberNameInfo().getAsString();
		Expr* base = Stmt->getBase();
		if (isStdArray(base->getType()) && memberName == "assign")
		{
			TraverseStmt(base);
			out() << "[] = ";
			return true;
		}
		if (base->getStmtClass() != Stmt::StmtClass::CXXThisExprClass)
		{
			TraverseStmt(base);
			out() << '.';
		}
		auto const kind = declName.getNameKind();
		if (kind == DeclarationName::NameKind::CXXConversionFunctionName)
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
		if (tmpArgCount != 0)
		{
			bool const needParen = (tmpArgCount > 1) || (tmpArgCount == 1 && tempArgHasTempArg(TAL[0].getArgument()));
			out() << (needParen? "!(": "!");
			for (unsigned I = 0; I < tmpArgCount; ++I)
			{
				spliter.split();
				TraverseTemplateArgumentLoc(TAL[I]);
			}
			out() << (needParen ? ")" : "");
		}

		return true;
	}

	bool TraverseCXXMemberCallExpr(CXXMemberCallExpr* Stmt)
	{
		TraverseStmt(Stmt->getCallee());
		out() << '(';
		Spliter spliter(", ");
		for (auto c : Stmt->arguments())
		{
			if (c->getStmtClass() != Stmt::StmtClass::CXXDefaultArgExprClass)
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

	bool TraverseCompoundAssignOperator(CompoundAssignOperator* op)
	{
		VisitorToD::TraverseBinaryOperator(op);
		return true;
	}

#define OPERATOR(NAME) bool TraverseBin##NAME##Assign(CompoundAssignOperator *S) {return TraverseCompoundAssignOperator(S);}
	OPERATOR(Mul) OPERATOR(Div) OPERATOR(Rem) OPERATOR(Add) OPERATOR(Sub)
	OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or) OPERATOR(Xor)
#undef OPERATOR
	

	bool TraverseSubstNonTypeTemplateParmExpr(SubstNonTypeTemplateParmExpr* Expr)
	{
		TraverseStmt(Expr->getReplacement());
		return true;
	}

	bool TraverseBinaryOperator(BinaryOperator* Stmt)
	{
		TraverseStmt(Stmt->getLHS());
		out() << " " << Stmt->getOpcodeStr().str() << " ";
		TraverseStmt(Stmt->getRHS());
		return true;
	}
#define OPERATOR(NAME) bool TraverseBin##NAME(BinaryOperator* Stmt) {return TraverseBinaryOperator(Stmt);}
	OPERATOR(PtrMemD) OPERATOR(PtrMemI) OPERATOR(Mul) OPERATOR(Div)
		OPERATOR(Rem) OPERATOR(Add) OPERATOR(Sub) OPERATOR(Shl) OPERATOR(Shr)
		OPERATOR(LT) OPERATOR(GT) OPERATOR(LE) OPERATOR(GE) OPERATOR(EQ)
		OPERATOR(NE) OPERATOR(And) OPERATOR(Xor) OPERATOR(Or) OPERATOR(LAnd)
		OPERATOR(LOr) OPERATOR(Assign) OPERATOR(Comma)
#undef OPERATOR

	bool TraverseUnaryOperator(UnaryOperator* Stmt)
	{
		if (Stmt->isPostfix())
		{
			//TraverseStmt(Stmt->getSubExpr());
			for (auto c : Stmt->children())
				TraverseStmt(c);
			out() << Stmt->getOpcodeStr(Stmt->getOpcode()).str();
		}
		else
		{
			out() << Stmt->getOpcodeStr(Stmt->getOpcode()).str();
			//TraverseStmt(Stmt->getSubExpr());
			for (auto c : Stmt->children())
				TraverseStmt(c);
		}
		return true;
	}
#define OPERATOR(NAME) bool TraverseUnary##NAME(UnaryOperator* Stmt) {return TraverseUnaryOperator(Stmt);}
	OPERATOR(PostInc) OPERATOR(PostDec) OPERATOR(PreInc) OPERATOR(PreDec)
		OPERATOR(AddrOf) OPERATOR(Deref) OPERATOR(Plus) OPERATOR(Minus)
		OPERATOR(Not) OPERATOR(LNot) OPERATOR(Real) OPERATOR(Imag)
		OPERATOR(Extension) OPERATOR(Coawait)
#undef OPERATOR

	bool TraverseDeclRefExpr(DeclRefExpr* Expr)
	{
		if (Expr->hasQualifier())
			TraverseNestedNameSpecifier(Expr->getQualifier());
		auto decl = Expr->getDecl();
		if (decl->getKind() == Decl::Kind::EnumConstant)
		{
			PrintType(decl->getType());
			out() << '.';
		}
		out() << mangleName(Expr->getNameInfo().getAsString());
		return true;
	}

	bool TraverseRecordType(RecordType* Type)
	{
		out() << mangleType(Type->getDecl());
		return true;
	}

	bool TraverseConstantArrayType(ConstantArrayType* Type)
	{
		PrintType(Type->getElementType());
		out() << '[' << Type->getSize().toString(10, false) << ']';
		return true;
	}

	bool TraverseInitListExpr(InitListExpr* Expr)
	{
		out() << "{ " << std::endl;
		++indent;
		for (auto c : Expr->inits())
		{
			out() << indent_str();
			TraverseStmt(c);
			out() << ',' << std::endl;
		}
		--indent;
		out() << indent_str() << "}";
		return true;
	}

	bool TraverseParenExpr(ParenExpr* Expr)
	{
		out() << '(';
		TraverseStmt(Expr->getSubExpr());
		out() << ')';
		return true;
	}

	void TraverseVarDeclImpl(VarDecl* Decl)
	{
		if (Decl->isOutOfLine())
			return;
		else if (Decl->getOutOfLineDefinition())
			Decl = Decl->getOutOfLineDefinition();

		if (Decl->isStaticDataMember() || Decl->isStaticLocal())
			out() << "static ";
		PrintType(Decl->getType());
		out() << " ";
		if (!Decl->isOutOfLine())
		{
			if (auto qualifier = Decl->getQualifier())
				TraverseNestedNameSpecifier(qualifier);
		}
		out() << mangleName(Decl->getNameAsString());
		bool const in_foreach_decl = receiver.forrange_loopvar.count(Decl) != 0;
		auto init = Decl->getInit();
		if (init && !in_foreach_decl)
		{
			if (Decl->isDirectInit())
			{
				if(init->getStmtClass() != Stmt::StmtClass::CXXConstructExprClass)
					TraverseStmt(init);
				else
				{
					auto const constr = static_cast<CXXConstructExpr*>(init);
					if (constr->getNumArgs() != 0)
					{
						out() << " = ";
						PrintCXXConstructExprParams(constr);
					}
				}
			}
			else
			{
				out() << " = ";
				TraverseStmt(init);
			}
		}
	}

	bool TraverseVarDecl(VarDecl* Decl)
	{
		//out() << indent_str();
		TraverseVarDeclImpl(Decl);
		//out() << ";" << std::endl << std::flush;
		return true;
	}

	/*bool TraverseType(QualType const& Type)
	{
		//out() << Type.getAsString();
		//if(Type.hasQualifiers())
		//Qualifiers quals = Type.getQualifiers();
		Base::TraverseType(Type);
		return true;
	}*/


	bool VisitDecl(Decl *Decl)
	{
		out() << indent_str() << "/*" << Decl->getDeclKindName() << " Decl*/";
		return true;
	}

	bool VisitStmt(Stmt *Stmt)
	{
		out() << indent_str() << "/*" << Stmt->getStmtClassName() << " Stmt*/";
		return true;
	}

	bool VisitType(Type *Type)
	{
		out() << indent_str() << "/*" << Type->getTypeClassName() << " Type*/";
		return true;
	}

	std::set<std::string> extern_includes;
	std::string modulename;

private:

	const char* getFile(Stmt const* d)
	{
		/*Module* module = d->getLocalOwningModule();
		if (clang::FileEntry const* f = module->getASTFile())
		return f->getName();
		else
		return nullptr;*/
		clang::SourceLocation sl = d->getLocStart();
		if (sl.isValid() == false)
			return "";
		clang::FullSourceLoc fsl = Context->getFullLoc(sl).getExpansionLoc();
		auto& mgr = fsl.getManager();
		if (clang::FileEntry const* f = mgr.getFileEntryForID(fsl.getFileID()))
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
		if (sl.isValid() == false)
			return "";
		clang::FullSourceLoc fsl = Context->getFullLoc(sl).getExpansionLoc();
		auto& mgr = fsl.getManager();
		if (clang::FileEntry const* f = mgr.getFileEntryForID(fsl.getFileID()))
			return f->getName();
		else
			return nullptr;
	}

	bool checkFilename(Decl const* d)
	{
		std::string filepath = getFile(d);
		if (filepath.size() < 12)
			return false;
		else
		{
			std::string exts[] = {
				std::string(".h"),
				std::string(".hpp"),
				std::string(".cpp"),
			};
			for (auto& ext : exts)
			{
				auto modulenameext = modulename + ext;
				auto filename = filepath.substr(filepath.size() - modulenameext.size());
				if (filename == modulenameext)
					return true;
			}
			return false;
		}
	}

	MatchContainer const& receiver;
	size_t indent = 0;
	ASTContext *Context;
	bool isCatchParameter = false;

	std::vector<std::vector<NamedDecl*> > template_args_stack;
};


class VisitorToDConsumer : public clang::ASTConsumer
{
public:
	explicit VisitorToDConsumer(
			ASTContext *Context, 
			llvm::StringRef InFile
		)
		: finder(getMatcher(receiver))
		, finderConsumer(finder.newASTConsumer())
		, InFile(InFile.data(), InFile.size())
		, Visitor(Context, receiver)
	{
	}

	virtual void HandleTranslationUnit(clang::ASTContext &Context)
	{
		finderConsumer->HandleTranslationUnit(Context);
		int lastSep = (int)InFile.find_last_of('/');
		if (lastSep == std::string::npos)
			lastSep = (int)InFile.find_last_of('\\');
		if (lastSep == std::string::npos)
			lastSep = -1;
		auto lastDot = InFile.find_last_of('.');
		if (lastDot == std::string::npos)
			lastDot = InFile.size();
		lastSep += 1;
		auto modulename = InFile.substr(lastSep, lastDot - lastSep);
		Visitor.modulename = modulename;
		Visitor.TraverseTranslationUnitDecl(Context.getTranslationUnitDecl());

		std::set<std::string> imports;
		for (auto const& include : Visitor.extern_includes)
			imports.insert(include);

		std::ofstream file(modulename + ".d");
		for (auto const& import : imports)
			file << "import " << import << ";" << std::endl;
		file << std::endl;
		file << out().str();
	}

private:
	MatchContainer receiver;
	ast_matchers::MatchFinder finder;
	std::unique_ptr<clang::ASTConsumer> finderConsumer;
	std::string InFile;
	VisitorToD Visitor;
};


class Find_Includes : public PPCallbacks
{
public:
	void InclusionDirective(
		SourceLocation,		//hash_loc,
		const Token&,		//include_token,
		StringRef file_name,
		bool,				//is_angled,
		CharSourceRange,	//filename_range,
		const FileEntry*,	//file,
		StringRef,			//search_path,
		StringRef,			//relative_path,
		const Module*		//imported
		)
	{
		includes_in_file.insert(file_name);
	}
};

std::unique_ptr<clang::ASTConsumer> VisitorToDAction::CreateASTConsumer(
	clang::CompilerInstance &Compiler,
	llvm::StringRef InFile
	)
{
	return std::make_unique<VisitorToDConsumer>(&Compiler.getASTContext(), InFile);
}

bool VisitorToDAction::BeginSourceFileAction(CompilerInstance &ci, StringRef)
{
	std::unique_ptr<Find_Includes> find_includes_callback(new Find_Includes());

	Preprocessor &pp = ci.getPreprocessor();
	pp.addPPCallbacks(std::move(find_includes_callback));

	return true;
}

void VisitorToDAction::EndSourceFileAction()
{
}
