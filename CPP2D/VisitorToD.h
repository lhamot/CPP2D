#pragma once

#pragma warning(push, 0)
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Decl.h>
#pragma warning(pop)
#include <unordered_map>

class MatchContainer;

extern std::set<std::string> includes_in_file;
extern std::vector<std::unique_ptr<std::stringstream> > outStack;

std::stringstream& out();

class VisitorToD : public clang::RecursiveASTVisitor<VisitorToD>
{
	typedef RecursiveASTVisitor<VisitorToD> Base;

	void include_file(std::string const& decl_inc);

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

	void PrintMacroArgs(clang::CallExpr* macro_args);

	void PrintStmtMacro(std::string const& varName, clang::Expr* init);

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

public:
	explicit VisitorToD(
	  clang::ASTContext* Context,
	  MatchContainer const& receiver,
	  llvm::StringRef file);

	std::string indent_str() const;

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

	void printTmpArgList(std::string const& tmpArgListStr);

	bool TraverseTemplateSpecializationType(clang::TemplateSpecializationType* Type);

	bool TraverseTypedefType(clang::TypedefType* Type);

	template<typename InitList>
	bool TraverseCompoundStmtImpl(clang::CompoundStmt* Stmt, InitList initList);

	bool TraverseCompoundStmt(clang::CompoundStmt* Stmt);

	template<typename InitList>
	bool TraverseCXXTryStmtImpl(clang::CXXTryStmt* Stmt, InitList initList);

	bool TraverseCXXTryStmt(clang::CXXTryStmt* Stmt);

	bool pass_decl(clang::Decl* decl);

	bool pass_stmt(clang::Stmt* stmt);

	bool TraverseNamespaceDecl(clang::NamespaceDecl* Decl);

	bool TraverseCXXCatchStmt(clang::CXXCatchStmt* Stmt);

	bool TraverseAccessSpecDecl(clang::AccessSpecDecl* Decl);

	void printBasesClass(clang::CXXRecordDecl* decl);

	bool TraverseCXXRecordDecl(clang::CXXRecordDecl* decl);

	bool TraverseRecordDecl(clang::RecordDecl* Decl);

	template<typename TmpSpecFunc, typename PrintBasesClass>
	bool TraverseCXXRecordDeclImpl(
	  clang::RecordDecl* decl,
	  TmpSpecFunc traverseTmpSpecs,
	  PrintBasesClass printBasesClass);

	void PrintTemplateParameterList(
	  clang::TemplateParameterList* tmpParams,
	  std::string const& prevTmplParmsStr);

	bool TraverseClassTemplateDecl(clang::ClassTemplateDecl* Decl);

	clang::TemplateParameterList* getTemplateParameters(clang::ClassTemplateSpecializationDecl*);

	clang::TemplateParameterList* getTemplateParameters(clang::ClassTemplatePartialSpecializationDecl* Decl);

	bool TraverseClassTemplatePartialSpecializationDecl(
	  clang::ClassTemplatePartialSpecializationDecl* Decl);

	bool TraverseClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* Decl);

	void PrintTemplateArgument(clang::TemplateArgument const& ta);

	void PrintTemplateSpec_TmpArgsAndParms(
	  clang::TemplateParameterList& primaryTmpParams,
	  clang::TemplateArgumentList const& tmpArgs,
	  clang::TemplateParameterList* newTmpParams,
	  std::string const& prevTmplParmsStr
	);

	template<typename D>
	bool TraverseClassTemplateSpecializationDeclImpl(D* Decl);

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

	void PrintCXXConstructExprParams(clang::CXXConstructExpr* Init);

	bool TraverseCXXConstructExpr(clang::CXXConstructExpr* Init);

	void PrintType(clang::QualType const& type);

	bool TraverseConstructorInitializer(clang::CXXCtorInitializer* Init);

	void startCtorBody(clang::FunctionDecl*);

	void startCtorBody(clang::CXXConstructorDecl* Decl);

	void printFuncEnd(clang::CXXMethodDecl* Decl);

	void printFuncEnd(clang::FunctionDecl*);

	void printSpecialMethodAttribute(clang::CXXMethodDecl* Decl);

	bool printFuncBegin(clang::CXXMethodDecl* Decl, std::string& tmpParams, int arg_become_this = -1);

	bool printFuncBegin(clang::FunctionDecl* Decl, std::string& tmpParams, int arg_become_this = -1);

	bool printFuncBegin(clang::CXXConversionDecl* Decl, std::string& tmpParams, int = -1);

	bool printFuncBegin(clang::CXXConstructorDecl* Decl,
	                    std::string&,	//tmpParams
	                    int = -1		//arg_become_this = -1
	                   );

	bool printFuncBegin(clang::CXXDestructorDecl*,
	                    std::string&,	//tmpParams,
	                    int = -1		//arg_become_this = -1
	                   );


	template<typename D>
	bool TraverseFunctionDeclImpl(
	  D* Decl,
	  int arg_become_this = -1);

	bool TraverseUsingDecl(clang::UsingDecl* Decl);

	bool TraverseFunctionDecl(clang::FunctionDecl* Decl);

	bool TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl* Decl);

	bool TraverseFunctionTemplateDecl(clang::FunctionTemplateDecl* Decl);

	bool TraverseBuiltinType(clang::BuiltinType* Type);

	enum Semantic
	{
		Value,
		Reference,
	};
	Semantic getSemantic(clang::QualType qt);

	template<typename PType>
	bool TraversePointerTypeImpl(PType* Type);

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

	std::set<clang::Expr* > dont_take_ptr;

	bool TraverseCallExpr(clang::CallExpr* Stmt);

	bool TraverseImplicitCastExpr(clang::ImplicitCastExpr* Stmt);

	bool TraverseCXXThisExpr(clang::CXXThisExpr* expr);

	bool isStdArray(clang::QualType const& type);

	bool isStdUnorderedMap(clang::QualType const& type);

	bool TraverseCXXDependentScopeMemberExpr(clang::CXXDependentScopeMemberExpr* expr);

	bool TraverseMemberExpr(clang::MemberExpr* Stmt);

	template<typename ME>
	bool TraverseMemberExprImpl(ME* Stmt);

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

	template<typename TDeclRefExpr>
	bool TraverseDeclRefExprImpl(TDeclRefExpr* Expr);

	bool TraverseDeclRefExpr(clang::DeclRefExpr* Expr);

	bool TraverseDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr* expr);

	bool TraverseRecordType(clang::RecordType* Type);

	bool TraverseConstantArrayType(clang::ConstantArrayType* Type);

	bool TraverseIncompleteArrayType(clang::IncompleteArrayType* Type);

	bool TraverseInitListExpr(clang::InitListExpr* Expr);

	bool TraverseParenExpr(clang::ParenExpr* expr);

	bool TraverseImplicitValueInitExpr(clang::ImplicitValueInitExpr* expr);

	void TraverseVarDeclImpl(clang::VarDecl* Decl);

	bool TraverseVarDecl(clang::VarDecl* Decl);

	bool VisitDecl(clang::Decl* Decl);

	bool VisitStmt(clang::Stmt* Stmt);

	bool VisitType(clang::Type* Type);

	std::set<std::string> extern_includes;
	std::string modulename;
	std::string insert_after_import;

private:

	const char* getFile(clang::Stmt const* d);

	const char* getFile(clang::Decl const* d);

	bool checkFilename(clang::Decl const* d);

	MatchContainer const& receiver;
	size_t indent = 0;
	clang::ASTContext* Context;
	size_t isInMacro = 0;

	std::vector<std::vector<clang::NamedDecl* > > template_args_stack;
	std::unordered_map<clang::IdentifierInfo*, std::string> renamedIdentifiers;
	std::unordered_map<clang::CXXRecordDecl*, ClassInfo> classInfoMap;
	bool renameIdentifiers = true;
	bool refAccepted = false;
	bool inFuncArgs = false;
	bool inForRangeInit = false;
};
