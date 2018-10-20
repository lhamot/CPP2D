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
//! Get the name of the file pointed by sl
const char* getFile(clang::SourceManager const& sourceManager, clang::SourceLocation const& sl);
//! Get the name of the file pointed by s
const char* getFile(clang::SourceManager const& sourceManager, clang::Stmt const* s);
//! Get the name of the file pointed by d
const char* getFile(clang::SourceManager const& sourceManager, clang::Decl const* d);
//! @return true if the file is the header of the module (the matching header of the cpp)
bool checkFilename(std::string const& modulename, char const* filepath_str);
//! @return true if the file of d is in the header of the module
bool checkFilename(clang::SourceManager const& sourceManager,
                   std::string const& modulename,
                   clang::Decl const* d);

//! @brief Replace the ocurances of search in subject, by replace
//! @return subject with the replaced strings.
std::string replaceString(std::string subject,
                          const std::string& search,
                          const std::string& replace);

}