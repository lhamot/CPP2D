//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <unordered_map>
#include <stack>
#include <map>
#include <set>

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/PrettyPrinter.h>
#pragma warning(pop)

#include "Options.h"

class MatchContainer;

//! Visit all the AST and print the compilation unit into **D** language file
class DPrinter : public clang::RecursiveASTVisitor<DPrinter>
{
	typedef RecursiveASTVisitor<DPrinter> Base;

public:
	explicit DPrinter(
	  clang::ASTContext* Context,
	  MatchContainer const& receiver,
	  llvm::StringRef file);

	//! Set the list if #include found in the C++ source
	void setIncludes(std::set<std::string> const& includes);

	//! Get indentation string for a new line in **D** code
	std::string indentStr() const;

	//! Print in **dlang** the base class part of a class declaration. Like class A <b>: B</b>
	void printBasesClass(clang::CXXRecordDecl* decl);

	//! Print in **dlang** the template argument of a template func/type. Like A<b>!(B)</b>
	void printTmpArgList(std::string const& tmpArgListStr);

	//! Print in **dlang** the common part of RecordDecl, CXXRecordDecl, and template records
	template<typename TmpSpecFunc, typename PrintBasesClass>
	void traverseCXXRecordDeclImpl(
	  clang::RecordDecl* decl,
	  TmpSpecFunc traverseTmpSpecs,		//!< A callable printing the template parameters. Like struct A<b>(C, int)</b>
	  PrintBasesClass printBasesClass	//!< A callable printing the base classes. Like class A <b>: B</b>
	);

	//! Print the template parameters of a struct/func declaration. Like struct A<b>(C, int)</b>
	void printTemplateParameterList(
	  clang::TemplateParameterList* tmpParams,
	  std::string const& prevTmplParmsStr);

	//! Print a clang::CompoundStmt with an optional function for print an initialization list
	template<typename InitList, typename AddBeforeEnd>
	void traverseCompoundStmtImpl(
	  clang::CompoundStmt* Stmt, //!< The clang::CompoundStmt to print
	  InitList initList, //!< A callable printing somthing a the begining of the scope
	  AddBeforeEnd addBeforEnd //!< A callable printing somthing a the end of the scope
	);

	//! Print a clang::CXXTryStmt with an optional function for print an initialization list
	template<typename InitList>
	void traverseCXXTryStmtImpl(
	  clang::CXXTryStmt* Stmt, //!< The clang::CXXTryStmt to print
	  InitList initList //!< A callable printing somthing a the begining of the scope
	);

	//! Get TemplateParameterList* of Decl if Decl is a clang::ClassTemplatePartialSpecializationDecl
	clang::TemplateParameterList* getTemplateParameters(
	  clang::ClassTemplateSpecializationDecl* Decl);

	//! Get TemplateParameterList* of Decl if Decl is a clang::ClassTemplatePartialSpecializationDecl
	clang::TemplateParameterList* getTemplateParameters(
	  clang::ClassTemplatePartialSpecializationDecl* Decl);

	//! Print to **dlang** the clang::TemplateArgument ta
	void printTemplateArgument(clang::TemplateArgument const& ta);

	//! Print the template part a of tmpl specialization. Like class A<b>(B = int, int = 2, C)</b>
	void printTemplateSpec_TmpArgsAndParms(
	  clang::TemplateParameterList& primaryTmpParams, //!< Tmpl params of the specialized template
	  clang::TemplateArgumentList const& tmpArgs, //!< Tmpl arguments of the specialization
	  const clang::ASTTemplateArgumentListInfo* tmpArgsInfo, //!< Tmpl arguments of the specialization
	  clang::TemplateParameterList* newTmpParams, //!< [optional] Tmpl params of the specialization
	  std::string const& prevTmplParmsStr //!< String to add before the first param
	);

	//! Print to **dlang** the construction expr in this way ```Type(arg1, arg2)```
	void printCXXConstructExprParams(clang::CXXConstructExpr* Init);

	//! Print a type name to **dlang**
	void printType(clang::QualType const& type);

	//! Trait printing the initialization list of the clang::FunctionDecl (nothing)
	void startCtorBody(clang::FunctionDecl*);

	//! Trait printing the initialization list of the clang::CXXConstructorDecl
	void startCtorBody(clang::CXXConstructorDecl* Decl);

	//! Trait printing the const keyword (nothing) at the end of the clang::FunctionDecl prototype
	void printFuncEnd(clang::FunctionDecl* decl);

	//! Trait printing the const keyword at the end of the clang::CXXMethodDecl prototype
	void printFuncEnd(clang::CXXMethodDecl* Decl);

	//! Print keywords of a method. Like **static**, **abstract**, **override**, **final**
	void printSpecialMethodAttribute(clang::CXXMethodDecl* Decl);

	//! @brief Print the return type and name of a function. Like **rtype func_name**(arg1, arg2)
	//! @return true if this Decl has to be printed, else false
	bool printFuncBegin(
	  clang::FunctionDecl* Decl,
	  std::string& tmpParams,
	  int thisArgIndex
	);

	//! @brief Print the return type and name of a method. Like **rtype method_name**(arg1, arg2)
	//! @overload bool DPrinter::printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int thisArgIndex)
	bool printFuncBegin(clang::CXXMethodDecl* Decl, std::string& tmpParams, int thisArgIndex);

	//! @brief Print the return type and name of a function. Like **T opCast**(T)()
	//! @overload bool DPrinter::printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int thisArgIndex)
	bool printFuncBegin(clang::CXXConversionDecl* Decl, std::string& tmpParams, int thisArgIndex);

	//! @brief Print the return type and name of ctor. Like **this**(arg1)
	//! @overload bool DPrinter::printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int thisArgIndex)
	bool printFuncBegin(clang::CXXConstructorDecl* Decl,
	                    std::string& tmpParams,
	                    int thisArgIndex);

	//! @brief Print the return type and name of dtor. Like <b>~this</b>()
	//! @overload bool DPrinter::printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int thisArgIndex)
	bool printFuncBegin(clang::CXXDestructorDecl*, std::string& tmpParams, int thisArgIndex);

	//! @brief Common function to print function
	//! @param Decl FunctionDel to print
	//! @param thisArgIndex This parameter will be "this".\n
	//!        Usefull to put free operators inside, because one operand become "this".\n
	//!        -1 mean no parameter is changed.
	template<typename D>
	void traverseFunctionDeclImpl(D* Decl, int thisArgIndex = -1);

	//! Get the semantic of the passed type
	static TypeOptions::Semantic getSemantic(clang::QualType qt);

	//! Is this type a pointer, or a smart pointer
	static bool isPointer(clang::QualType const&);

	//! @brief Print a <b>C++</b> pointer type to **D**
	//! Pointer to TypeOptions::Value become T[], and Semantic::Reference become just T.
	template<typename PType>
	void traversePointerTypeImpl(PType* Type);

	//! Common printing code for TraverseClassTemplate(Partial)SpecializationDecl
	template<typename D>
	void traverseClassTemplateSpecializationDeclImpl(
	  D* Decl,
	  const clang::ASTTemplateArgumentListInfo* tmpArgsInfo //!< Optional
	);

	//! Is this clang::QualType a std::array or a boost::array ?
	static bool isStdArray(clang::QualType const& type);

	//! Is this clang::QualType a std::unordered_map or a boost::unordered_map ?
	static bool isStdUnorderedMap(clang::QualType const& type);

	//! @brief Print the template args for (DependentScope)DeclRefExpr and UnresolvedLookupExpr
	//! Example : func<b>!(T1, T2)</b>
	template<typename ME>
	void traverseDeclRefExprImpl(ME* Expr);

	//! Print a (CXXDependentScope)MemberExpr
	template<typename ME>
	bool traverseMemberExprImpl(ME* Stmt);

	//! Get the list of all **import** to print in **D**
	std::map<std::string, std::set<std::string>> const& getExternIncludes() const;

	//! Get the printer **D** code
	std::string getDCode() const;

	//! Get the std::ostream where print **D** code.
	std::ostream& stream();

	//! Insert this include to the needed imports
	void addExternInclude(
	  std::string const& include, //!< Modulename to import
	  std::string const& typeName //!< Hint to known why this module is imported
	);

	//! Print arguments of (CXXMember)CallExpr
	void printCallExprArgument(clang::CallExpr* Stmt);

	//! Print clang::VarDecl, also in CXXForRangeStmt and catch
	void traverseVarDeclImpl(clang::VarDecl* Decl);

	//! Print a statment which can be a compound one, a normal one, or a NullStmt (printed to {})
	void traverseCompoundStmtOrNot(clang::Stmt* Stmt);

	//! Check if decl is the type named baseName, or inherit from it directly or indirectly
	static bool isA(clang::CXXRecordDecl* decl, std::string const& baseName);

	//  ******************** Function called by RecursiveASTVisitor *******************************
	bool shouldVisitImplicitCode() const;

	//! @pre setIncludes has already been called before
	bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl* Decl);

	bool TraverseTypedefDecl(clang::TypedefDecl* Decl);

	bool TraverseTypeAliasDecl(clang::TypeAliasDecl* Decl);

	bool TraverseTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* Decl);

	bool TraverseFieldDecl(clang::FieldDecl* Decl);

	bool TraverseDependentNameType(clang::DependentNameType* Type);

	bool TraverseAttributedType(clang::AttributedType* Type);

	bool TraverseDecayedType(clang::DecayedType* Type);

	bool TraverseElaboratedType(clang::ElaboratedType* Type);

	bool TraverseInjectedClassNameType(clang::InjectedClassNameType* Type);

	bool TraverseSubstTemplateTypeParmType(clang::SubstTemplateTypeParmType*);

	bool TraverseNestedNameSpecifier(clang::NestedNameSpecifier* NNS);

	bool TraverseTemplateSpecializationType(clang::TemplateSpecializationType* Type);

	bool TraverseTypedefType(clang::TypedefType* Type);

	bool TraverseCompoundStmt(clang::CompoundStmt* Stmt);

	bool TraverseCXXTryStmt(clang::CXXTryStmt* Stmt);

	bool TraverseNamespaceDecl(clang::NamespaceDecl* Decl);

	bool TraverseCXXCatchStmt(clang::CXXCatchStmt* Stmt);

	bool TraverseAccessSpecDecl(clang::AccessSpecDecl* Decl);

	bool TraverseCXXRecordDecl(clang::CXXRecordDecl* decl);

	bool TraverseRecordDecl(clang::RecordDecl* Decl);

	bool TraverseClassTemplateDecl(clang::ClassTemplateDecl* Decl);

	bool TraverseClassTemplatePartialSpecializationDecl(
	  clang::ClassTemplatePartialSpecializationDecl* Decl);

	bool TraverseClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* Decl);

	bool TraverseCXXConversionDecl(clang::CXXConversionDecl* Decl);

	bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* Decl);

	bool TraverseCXXDestructorDecl(clang::CXXDestructorDecl* Decl);

	bool TraverseCXXMethodDecl(clang::CXXMethodDecl* Decl);

	bool TraversePredefinedExpr(clang::PredefinedExpr* expr);

	bool TraverseCXXDefaultArgExpr(clang::CXXDefaultArgExpr* expr);

	bool TraverseCXXUnresolvedConstructExpr(clang::CXXUnresolvedConstructExpr*   Expr);

	bool TraverseUnresolvedLookupExpr(clang::UnresolvedLookupExpr*   Expr);

	bool TraverseCXXForRangeStmt(clang::CXXForRangeStmt*   Stmt);

	bool TraverseDoStmt(clang::DoStmt*   Stmt);

	bool TraverseSwitchStmt(clang::SwitchStmt* Stmt);

	bool TraverseCaseStmt(clang::CaseStmt* Stmt);

	bool TraverseBreakStmt(clang::BreakStmt* Stmt);

	bool TraverseContinueStmt(clang::ContinueStmt* Stmt);

	bool TraverseStaticAssertDecl(clang::StaticAssertDecl* Decl);

	bool TraverseDefaultStmt(clang::DefaultStmt* Stmt);

	bool TraverseCXXDeleteExpr(clang::CXXDeleteExpr* Expr);

	bool TraverseCXXNewExpr(clang::CXXNewExpr* Expr);

	bool TraverseCXXConstructExpr(clang::CXXConstructExpr* Init);

	bool TraverseConstructorInitializer(clang::CXXCtorInitializer* Init);

	bool TraverseUsingDecl(clang::UsingDecl* Decl);

	bool TraverseFunctionDecl(clang::FunctionDecl* Decl);

	bool TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl* Decl);

	bool TraverseFunctionTemplateDecl(clang::FunctionTemplateDecl* Decl);

	bool TraverseBuiltinType(clang::BuiltinType* Type);

	bool TraverseMemberPointerType(clang::MemberPointerType* Type);

	bool TraversePointerType(clang::PointerType* Type);

	bool TraverseCXXNullPtrLiteralExpr(clang::CXXNullPtrLiteralExpr* Expr);

	bool TraverseEnumConstantDecl(clang::EnumConstantDecl* Decl);

	bool TraverseEnumDecl(clang::EnumDecl* Decl);

	bool TraverseEnumType(clang::EnumType* Type);

	bool TraverseIntegerLiteral(clang::IntegerLiteral* Stmt);

	bool TraverseDecltypeType(clang::DecltypeType* Type);

	bool TraverseAutoType(clang::AutoType*);

	bool TraverseLinkageSpecDecl(clang::LinkageSpecDecl* Decl);

	bool TraverseFriendDecl(clang::FriendDecl* Decl);

	bool TraverseParmVarDecl(clang::ParmVarDecl* Decl);

	bool TraverseRValueReferenceType(clang::RValueReferenceType* Type);

	bool TraverseLValueReferenceType(clang::LValueReferenceType* Type);

	bool TraverseTemplateTypeParmType(clang::TemplateTypeParmType* Type);

	bool TraversePackExpansionType(clang::PackExpansionType* Type);

	bool TraversePackExpansionExpr(clang::PackExpansionExpr* expr);

	bool TraverseTemplateTypeParmDecl(clang::TemplateTypeParmDecl* Decl);

	bool TraverseNonTypeTemplateParmDecl(clang::NonTypeTemplateParmDecl* Decl);

	bool TraverseDeclStmt(clang::DeclStmt* Stmt);

	bool TraverseNamespaceAliasDecl(clang::NamespaceAliasDecl* Decl);

	bool TraverseReturnStmt(clang::ReturnStmt* Stmt);

	bool TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* Stmt);

	bool TraverseExprWithCleanups(clang::ExprWithCleanups* Stmt);

	bool TraverseArraySubscriptExpr(clang::ArraySubscriptExpr* Expr);

	bool TraverseFloatingLiteral(clang::FloatingLiteral* Expr);

	bool TraverseForStmt(clang::ForStmt* Stmt);

	bool TraverseWhileStmt(clang::WhileStmt* Stmt);

	bool TraverseIfStmt(clang::IfStmt* Stmt);

	bool TraverseCXXBindTemporaryExpr(clang::CXXBindTemporaryExpr* Stmt);

	bool TraverseCXXThrowExpr(clang::CXXThrowExpr* Stmt);

	bool TraverseMaterializeTemporaryExpr(clang::MaterializeTemporaryExpr* Stmt);

	bool TraverseCXXFunctionalCastExpr(clang::CXXFunctionalCastExpr* Stmt);

	bool TraverseParenType(clang::ParenType* Type);

	bool TraverseFunctionProtoType(clang::FunctionProtoType* Type);

	bool TraverseCXXTemporaryObjectExpr(clang::CXXTemporaryObjectExpr* Stmt);

	bool TraverseNullStmt(clang::NullStmt* Stmt);

	bool TraverseCharacterLiteral(clang::CharacterLiteral* Stmt);

	bool TraverseStringLiteral(clang::StringLiteral* Stmt);

	bool TraverseCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr* Stmt);

	bool TraverseUnaryExprOrTypeTraitExpr(clang::UnaryExprOrTypeTraitExpr* Expr);

	bool TraverseEmptyDecl(clang::EmptyDecl* Decl);

	bool TraverseLambdaExpr(clang::LambdaExpr* S);

	bool TraverseCallExpr(clang::CallExpr* Stmt);

	bool TraverseImplicitCastExpr(clang::ImplicitCastExpr* Stmt);

	bool TraverseCXXThisExpr(clang::CXXThisExpr* expr);

	bool TraverseCXXDependentScopeMemberExpr(clang::CXXDependentScopeMemberExpr* expr);

	bool TraverseMemberExpr(clang::MemberExpr* Stmt);

	bool TraverseCXXMemberCallExpr(clang::CXXMemberCallExpr* Stmt);

	bool TraverseCXXStaticCastExpr(clang::CXXStaticCastExpr* Stmt);

	bool TraverseCStyleCastExpr(clang::CStyleCastExpr* Stmt);

	bool TraverseConditionalOperator(clang::ConditionalOperator* op);

	bool TraverseCompoundAssignOperator(clang::CompoundAssignOperator* op);

	bool TraverseBinAddAssign(clang::CompoundAssignOperator* expr);

#define OPERATOR(NAME) bool TraverseBin##NAME##Assign(clang::CompoundAssignOperator *S);
	OPERATOR(Mul) OPERATOR(Div) OPERATOR(Rem) OPERATOR(Sub)
	OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or) OPERATOR(Xor)
#undef OPERATOR

	bool TraverseSubstNonTypeTemplateParmExpr(clang::SubstNonTypeTemplateParmExpr* Expr);

	bool TraverseBinaryOperator(clang::BinaryOperator* Stmt);

	bool TraverseBinAdd(clang::BinaryOperator* expr);

#define OPERATOR(NAME) bool TraverseBin##NAME(clang::BinaryOperator* Stmt);
	OPERATOR(PtrMemD) OPERATOR(PtrMemI) OPERATOR(Mul) OPERATOR(Div)
	OPERATOR(Rem) OPERATOR(Sub) OPERATOR(Shl) OPERATOR(Shr)
	OPERATOR(LT) OPERATOR(GT) OPERATOR(LE) OPERATOR(GE) OPERATOR(EQ)
	OPERATOR(NE) OPERATOR(And) OPERATOR(Xor) OPERATOR(Or) OPERATOR(LAnd)
	OPERATOR(LOr) OPERATOR(Assign) OPERATOR(Comma)
#undef OPERATOR

	bool TraverseUnaryOperator(clang::UnaryOperator* Stmt);

#define OPERATOR(NAME) bool TraverseUnary##NAME(clang::UnaryOperator* Stmt);
	OPERATOR(PostInc) OPERATOR(PostDec) OPERATOR(PreInc) OPERATOR(PreDec)
	OPERATOR(AddrOf) OPERATOR(Deref) OPERATOR(Plus) OPERATOR(Minus)
	OPERATOR(Not) OPERATOR(LNot) OPERATOR(Real) OPERATOR(Imag)
	OPERATOR(Extension) OPERATOR(Coawait)
#undef OPERATOR

	bool TraverseDeclRefExpr(clang::DeclRefExpr* Expr);

	bool TraverseDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr* expr);

	bool TraverseRecordType(clang::RecordType* Type);

	bool TraverseConstantArrayType(clang::ConstantArrayType* Type);

	bool TraverseIncompleteArrayType(clang::IncompleteArrayType* Type);

	bool TraverseDependentSizedArrayType(clang::DependentSizedArrayType* Type);

	bool TraverseInitListExpr(clang::InitListExpr* Expr);

	bool TraverseParenExpr(clang::ParenExpr* expr);

	bool TraverseImplicitValueInitExpr(clang::ImplicitValueInitExpr* expr);

	bool TraverseParenListExpr(clang::ParenListExpr* Expr);

	bool TraverseCXXScalarValueInitExpr(clang::CXXScalarValueInitExpr* Expr);

	bool TraverseUnresolvedMemberExpr(clang::UnresolvedMemberExpr* Expr);

	bool TraverseVarDecl(clang::VarDecl* Decl);

	bool TraverseIndirectFieldDecl(clang::IndirectFieldDecl*);

	bool VisitDecl(clang::Decl* Decl);

	bool VisitStmt(clang::Stmt* Stmt);

	bool VisitType(clang::Type* Type);

	std::set<clang::Expr*> dontTakePtr;    //!< Avoid to take pointer when implicit FunctionToPointerDecay

private:
	//! Add the import of for this file if it was included in the C++ file
	void includeFile(std::string const& inclFile, std::string const& typeName);

	//! Print the context (namespace, class, function) to **D**
	void printDeclContext(clang::DeclContext* DC);

	//! Get type name and transform it for **D** printing
	std::string printDeclName(clang::NamedDecl* decl);

	//! Print the comment preceding this clang::Decl
	void printCommentBefore(clang::Decl* t);

	//! Print the comment after this clang::Decl
	void printCommentAfter(clang::Decl* t);

	//! Trim from begin and end
	static std::string trim(std::string const& s);

	static std::vector<std::string> split_lines(std::string const& instr);

	//! Print comment before a clang::Stmt
	bool printStmtComment(
	  clang::SourceLocation& locStart,      //!< IN/OUT Comment start (Will become nextStart)
	  clang::SourceLocation const& locEnd,  //!< Comment end (Stmt start)
	  clang::SourceLocation const& nextStart = clang::SourceLocation(),  //!< Stmt end
	  bool doIndent = false   //!< Add indentation before each new line
	);

	//! Call a custom type printer for this type if it exist
	//! @return true if a custom printer was called
	bool customTypePrinter(clang::NamedDecl* decl);

	//! @brief Print to **D**, arguments of a **cpp2d_dummy_variadic** function call
	//! @see https://github.com/lhamot/CPP2D/wiki/Macro-migration
	void printMacroArgs(clang::CallExpr* macroArgs //!< Argument of the **cpp2d_dummy_variadic**
	                   );

	//! @brief Print a "stmt-style" macro
	//! @see https://github.com/lhamot/CPP2D/wiki/Macro-migration
	void printStmtMacro(
	  std::string const& varName, //!< CPP2D_MACRO_STMT or CPP2D_MACRO_STMT_END
	  clang::Expr* init           //!< Initialisation of the CPP2D_MACRO_STMT object
	);

	//! Info about the relation between two type
	struct RelationInfo
	{
		bool hasOpEqual = false; //!< If ```a == b``` is valid
		bool hasOpLess = false;  //!< If ```a < b``` is valid
	};
	//! Info about a type
	struct ClassInfo
	{
		std::unordered_map<clang::Type const*, RelationInfo> relations; //!< Relation with other types
		bool hasOpExclaim = false;   //!< If ```!a``` is valid
		bool hasBoolConv = false;    //!< If has an operator ```operator bool()```
	};
	//!< Using Custom matchers and custom printer (in MatchContainer) decide to custom print or not
	bool passDecl(clang::Decl* decl);
	//!< Using Custom matchers and custom printer (in MatchContainer) decide to custom print or not
	bool passStmt(clang::Stmt* stmt);
	//!< Using Custom matchers and custom printer (in MatchContainer) decide to custom print or not
	bool passType(clang::Type* type);

	std::set<std::string> includesInFile;  //!< All includes find in the <b>C++</b> file
	std::map<std::string, std::set<std::string> > externIncludes; //!< import to do in **D**
	std::string modulename; //!< Name of the <b>C++</b> module

	MatchContainer const& receiver; //!< Custom matchers and custom printers
	size_t indent = 0;              //!< Indentation level
	clang::ASTContext* Context;
	size_t isInMacro = 0;       //!< Disable printing if inside a macro expantion

	std::map<unsigned int, std::map<unsigned int, clang::NamedDecl* > > templateArgsStack; //!< Template argument names
	std::unordered_map<clang::IdentifierInfo*, std::string> renamedIdentifiers; //!< Avoid name collision
	std::unordered_map<clang::CXXRecordDecl*, ClassInfo> classInfoMap; //!< Info about all clang::CXXRecordDecl
	bool renameIdentifiers = true;  //!< Use renamedIdentifiers ?
	bool refAccepted = false; //!< If we are in a **D** place where we can use **ref**
	bool inFuncParams = false;  //!< We are printing function parameters
	bool inForRangeInit = false;  //!< If in for-range-base-loop initialization
	bool doPrintType = true;  //!< Do not re-print type in if multi-decl one line
	bool splitMultiLineDecl = true;  //!< If split multi line declaration is needed. False in for init.
	bool portConst = false;          //!< True to port **const** keyword.
	bool printDefaultValue = true;
	bool isThisFunctionUsefull = false; //!< To keep usefull implicit function
	bool inTemplateParamList = false;
	std::stack<std::string> catchedExceptNames;
	static clang::PrintingPolicy printingPolicy;  //!< Policy for print to C++ (sometimes useful)
};
