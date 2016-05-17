//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <unordered_map>

#pragma warning(push, 0)
#pragma warning(disable: 4265)
#include <clang/AST/RecursiveASTVisitor.h>
#pragma warning(pop)

class MatchContainer;

class DPrinter : public clang::RecursiveASTVisitor<DPrinter>
{
	typedef RecursiveASTVisitor<DPrinter> Base;

public:
	explicit DPrinter(
	  clang::ASTContext* Context,
	  MatchContainer const& receiver,
	  llvm::StringRef file);

	void setIncludes(std::set<std::string> const& includes);

	std::string indentStr() const;

	void printBasesClass(clang::CXXRecordDecl* decl);

	void printTmpArgList(std::string const& tmpArgListStr);

	template<typename TmpSpecFunc, typename PrintBasesClass>
	bool traverseCXXRecordDeclImpl(
	  clang::RecordDecl* decl,
	  TmpSpecFunc traverseTmpSpecs,
	  PrintBasesClass printBasesClass);

	void printTemplateParameterList(
	  clang::TemplateParameterList* tmpParams,
	  std::string const& prevTmplParmsStr);

	template<typename InitList>
	bool traverseCompoundStmtImpl(clang::CompoundStmt* Stmt, InitList initList);

	template<typename InitList>
	bool traverseCXXTryStmtImpl(clang::CXXTryStmt* Stmt, InitList initList);

	clang::TemplateParameterList* getTemplateParameters(clang::ClassTemplateSpecializationDecl*);

	clang::TemplateParameterList* getTemplateParameters(
	  clang::ClassTemplatePartialSpecializationDecl* Decl);

	void printTemplateArgument(clang::TemplateArgument const& ta);

	void printTemplateSpec_TmpArgsAndParms(
	  clang::TemplateParameterList& primaryTmpParams,
	  clang::TemplateArgumentList const& tmpArgs,
	  clang::TemplateParameterList* newTmpParams,
	  std::string const& prevTmplParmsStr
	);

	void printCXXConstructExprParams(clang::CXXConstructExpr* Init);

	void printType(clang::QualType const& type);

	void startCtorBody(clang::FunctionDecl*);

	void startCtorBody(clang::CXXConstructorDecl* Decl);

	void printFuncEnd(clang::CXXMethodDecl* Decl);

	void printFuncEnd(clang::FunctionDecl*);

	void printSpecialMethodAttribute(clang::CXXMethodDecl* Decl);

	bool printFuncBegin(clang::CXXMethodDecl* Decl, std::string& tmpParams, int thisArgIndex = -1);

	bool printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int thisArgIndex = -1);

	bool printFuncBegin(clang::CXXConversionDecl* Decl, std::string& tmpParams, int = -1);

	bool printFuncBegin(clang::CXXConstructorDecl* Decl,
	                    std::string& tmpParams,
	                    int thisArgIndex = -1);

	bool printFuncBegin(clang::CXXDestructorDecl*, std::string& tmpParams, int thisArgIndex = -1);

	template<typename D>
	bool traverseFunctionDeclImpl(D* Decl, int thisArgIndex = -1);

	enum Semantic
	{
		Value,
		Reference,
		AssocArray,  // Create without new, but reference semantics
	};
	static Semantic getSemantic(clang::QualType qt);

	template<typename PType>
	bool traversePointerTypeImpl(PType* Type);

	template<typename D>
	bool traverseClassTemplateSpecializationDeclImpl(D* Decl);

	static bool isStdArray(clang::QualType const& type);

	static bool isStdUnorderedMap(clang::QualType const& type);

	template<typename TDeclRefExpr>
	bool traverseDeclRefExprImpl(TDeclRefExpr* Expr);

	template<typename ME>
	bool traverseMemberExprImpl(ME* Stmt);

	std::map<std::string, std::set<std::string>> const& getExternIncludes() const;

	std::string getDCode() const;

	std::ostream& stream();

	void addExternInclude(std::string const& include, std::string const& typeName);

	void printCallExprArgument(clang::CallExpr* Stmt);

	void traverseVarDeclImpl(clang::VarDecl* Decl);

	//  ******************** Function called by RecursiveASTVisitor *******************************
	bool shouldVisitImplicitCode() const;

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

	bool TraverseTemplateTypeParmDecl(clang::TemplateTypeParmDecl* Decl);

	bool TraverseNonTypeTemplateParmDecl(clang::NonTypeTemplateParmDecl* Decl);

	bool TraverseDeclStmt(clang::DeclStmt* Stmt);

	bool TraverseNamespaceAliasDecl(clang::NamespaceAliasDecl* Decl);

	bool TraverseReturnStmt(clang::ReturnStmt* Stmt);

	bool TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* Stmt);

	bool TraverseExprWithCleanups(clang::ExprWithCleanups* Stmt);

	void TraverseCompoundStmtOrNot(clang::Stmt* Stmt);

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

	bool TraverseInitListExpr(clang::InitListExpr* Expr);

	bool TraverseParenExpr(clang::ParenExpr* expr);

	bool TraverseImplicitValueInitExpr(clang::ImplicitValueInitExpr* expr);

	bool TraverseParenListExpr(clang::ParenListExpr* Expr);

	bool TraverseVarDecl(clang::VarDecl* Decl);

	bool VisitDecl(clang::Decl* Decl);

	bool VisitStmt(clang::Stmt* Stmt);

	bool VisitType(clang::Type* Type);

private:
	void includeFile(std::string const& declInc, std::string const& typeName);

	std::string mangleType(clang::NamedDecl const* decl);

	std::string mangleVar(clang::DeclRefExpr* expr);

	std::string replace(std::string str, std::string const& in, std::string const& out);

	void printCommentBefore(clang::Decl* t);

	void printCommentAfter(clang::Decl* t);

	// trim from both ends
	static inline std::string trim(std::string const& s);

	std::vector<std::string> split(std::string const& instr);

	void printStmtComment(
	  clang::SourceLocation& locStart,
	  clang::SourceLocation const& locEnd,
	  clang::SourceLocation const& nextStart = clang::SourceLocation());

	void printMacroArgs(clang::CallExpr* macroArgs);

	void printStmtMacro(std::string const& varName, clang::Expr* init);

	struct RelationInfo
	{
		bool hasOpEqual = false;
		bool hasOpLess = false;
	};
	struct ClassInfo
	{
		std::unordered_map<clang::Type const*, RelationInfo> relations;
		bool hasOpExclaim = false;
		bool hasBoolConv = false;
	};
	bool passDecl(clang::Decl* decl);

	bool passStmt(clang::Stmt* stmt);

	bool passType(clang::Type* type);

	std::set<std::string> includesInFile;
	std::set<clang::Expr*> dontTakePtr;
	std::map<std::string, std::set<std::string> > externIncludes;
	std::string modulename;

	MatchContainer const& receiver;
	size_t indent = 0;
	clang::ASTContext* Context;
	size_t isInMacro = 0;

	std::vector<std::vector<clang::NamedDecl* > > templateArgsStack;
	std::unordered_map<clang::IdentifierInfo*, std::string> renamedIdentifiers;
	std::unordered_map<clang::CXXRecordDecl*, ClassInfo> classInfoMap;
	bool renameIdentifiers = true;
	bool refAccepted = false;
	bool inFuncArgs = false;
	bool inForRangeInit = false;
	bool doPrintType = true;
	bool splitMultiLineDecl = true;
	bool portConst = false;
	bool printDefaultValue = true;
	bool isThisFunctionUsefull = false;
};
