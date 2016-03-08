#include "Matcher.h"
#include "MatchContainer.h"

using namespace clang::ast_matchers;

MatchFinder getMatcher(MatchContainer& receiver)
{
	StatementMatcher foreachDeclInitMatcher =
	  cxxForRangeStmt(hasLoopVariable(varDecl(anything()).bind("forrange_loopvar")));

	StatementMatcher foreachDeclAutoMatcher =
	  cxxForRangeStmt(
	    hasLoopVariable(
	      varDecl(
	        anyOf(
	          hasType(lValueReferenceType(pointee(autoType().bind("forrange_loopvar_auto")))),
	          hasType(autoType().bind("forrange_loopvar_auto"))
	        ))));

	TypeMatcher refToClass =
	  lValueReferenceType(pointee(elaboratedType(namesType(recordType(
	                                hasDeclaration(cxxRecordDecl(isClass()))))))
	                     ).bind("ref_to_class");

	DeclarationMatcher hash_trait =
	  namespaceDecl(
	    allOf(
	      hasName("std"),
	      hasDescendant(
	        classTemplateSpecializationDecl(
	          allOf(
	            templateArgumentCountIs(1),
	            hasName("hash"),
	            hasMethod(cxxMethodDecl(hasName("operator()")).bind("hash_method"))
	          )
	        ).bind("dont_print_this_decl")
	      )
	    )
	  );

	DeclarationMatcher out_stream_op =
	  functionDecl(
	    unless(hasDeclContext(recordDecl())),
	    matchesName("operator.+"),
	    parameterCountIs(2)
	  ).bind("free_operator");

	MatchFinder finder;
	finder.addMatcher(foreachDeclInitMatcher, &receiver);
	finder.addMatcher(foreachDeclAutoMatcher, &receiver);
	finder.addMatcher(refToClass, &receiver);
	finder.addMatcher(hash_trait, &receiver);
	finder.addMatcher(out_stream_op, &receiver);
	return finder;
}