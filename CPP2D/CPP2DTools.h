//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once
#include <string>

namespace clang
{
class ASTContext;
class Stmt;
class Decl;
class SourceLocation;
}

namespace CPP2DTools
{
const char* getFile(clang::SourceManager const& sourceManager, clang::SourceLocation const& s);

const char* getFile(clang::SourceManager const& sourceManager, clang::Stmt const* d);

const char* getFile(clang::SourceManager const& sourceManager, clang::Decl const* d);

bool checkFilename(std::string const& modulename, char const* filepath_str);

bool checkFilename(clang::SourceManager const& sourceManager,
                   std::string const& modulename,
                   clang::Decl const* d);
}