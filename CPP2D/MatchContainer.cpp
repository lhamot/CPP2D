#include "MatchContainer.h"

using namespace clang;

void MatchContainer::run(const ast_matchers::MatchFinder::MatchResult &Result)
{
	if (VarDecl const* loopvar = Result.Nodes.getNodeAs<clang::VarDecl>("forrange_loopvar"))
		forrange_loopvar.insert(loopvar);

	if (AutoType const* type = Result.Nodes.getNodeAs<clang::AutoType>("forrange_loopvar_auto"))
		forrange_loopvar_auto.insert(type);
}
