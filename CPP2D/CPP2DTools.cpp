#pragma warning(push, 0)
#pragma warning(disable, 4702)
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
		return f->getName();
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
		std::string exts[] =
		{
			std::string(".h"),
			std::string(".hpp"),
			std::string(".cpp"),
			std::string(".cxx"),
			std::string(".c"),
		};
		for(auto& ext : exts)
		{
			std::string const modulenameext = modulename + ext;
			//if(modulenameext.size() > filepath.size())
			//	continue;
			if(filename == modulenameext)
				return true;
		}
		return false;
	}
}

bool checkFilename(clang::SourceManager const& sourceManager, std::string const& modulename, clang::Decl const* d)
{
	return checkFilename(modulename, getFile(sourceManager, d));
}
}