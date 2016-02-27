#include "MatchContainer.h"

#include <iostream>

using namespace clang;

void MatchContainer::run(const ast_matchers::MatchFinder::MatchResult &Result)
{
	if (auto* loopvar = Result.Nodes.getNodeAs<clang::VarDecl>("forrange_loopvar"))
		forrange_loopvar.insert(loopvar);

	if (auto* type = Result.Nodes.getNodeAs<clang::AutoType>("forrange_loopvar_auto"))
		forrange_loopvar_auto.insert(type);

	if (auto* type = Result.Nodes.getNodeAs<clang::LValueReferenceType>("ref_to_class"))
		ref_to_class.insert(type);

	if (auto* decl = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("hash_method"))
	{
		auto* tmplClass = 
			static_cast<ClassTemplateSpecializationDecl const*>(decl->getParent());
		auto&& tmpArgs = tmplClass->getTemplateArgs();
		if (tmpArgs.size() == 1)
		{
			auto const type_name = tmpArgs[0].getAsType().getCanonicalType().getAsString();
			hash_traits.emplace(type_name, decl);
		}
	}

	if (auto* decl = Result.Nodes.getNodeAs<clang::Decl>("dont_print_this_decl"))
		dont_print_this_decl.insert(decl);
}
