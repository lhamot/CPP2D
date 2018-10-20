//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma warning(push, 0)
#pragma warning(disable: 4548)
#include <llvm/Support/Path.h>
#include <clang/AST/ASTContext.h>
#pragma warning(pop)

using namespace llvm;
using namespace clang;


namespace CPP2DTools
{

const char* getFile(clang::SourceManager const& sourceManager, clang::SourceLocation const& sl)
{
	if(sl.isValid() == false)
		return "";
	clang::FullSourceLoc fsl = FullSourceLoc(sl, sourceManager).getExpansionLoc();
	auto& mgr = fsl.getManager();
	if(clang::FileEntry const* f = mgr.getFileEntryForID(fsl.getFileID()))
		return f->getName().data();
	else
		return nullptr;
}


const char* getFile(clang::SourceManager const& sourceManager, Stmt const* d)
{
	return getFile(sourceManager, d->getLocStart());
}

const char* getFile(clang::SourceManager const& sourceManager, Decl const* d)
{
	return getFile(sourceManager, d->getLocation());
}

bool checkFilename(std::string const& modulename, char const* filepath_str)
{
	if(filepath_str == nullptr)
		return false;
	std::string filepath = filepath_str;
	if(filepath.size() < 12)
		return false;
	else
	{
		StringRef const filename = llvm::sys::path::filename(filepath);
		static char const* exts[] = { ".h", ".hpp", ".cpp", ".cxx", ".c" };
		auto iter = std::find_if(std::begin(exts), std::end(exts), [&](auto && ext)
		{
			return filename == (modulename + ext);
		});
		return iter != std::end(exts);
	}
}

bool checkFilename(clang::SourceManager const& sourceManager,
                   std::string const& modulename,
                   clang::Decl const* d)
{
	return checkFilename(modulename, getFile(sourceManager, d));
}

std::string replaceString(std::string subject,
                          const std::string& search,
                          const std::string& replace)
{
	size_t pos = 0;
	while((pos = subject.find(search, pos)) != std::string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

} //CPP2DTools