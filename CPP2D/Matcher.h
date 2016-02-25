#pragma once

#pragma warning(push, 0)
#include "clang/ASTMatchers/ASTMatchFinder.h"
#pragma warning(pop)

class MatchContainer;

clang::ast_matchers::MatchFinder getMatcher(MatchContainer& receiver);