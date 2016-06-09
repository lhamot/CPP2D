#include <string>
#include "DPrinter.h"

struct Spliter
{
	DPrinter& printer;
	std::string const str;
	bool first = true;

	Spliter(DPrinter& pr, std::string const& s) : printer(pr), str(s) {}

	void split()
	{
		if(first)
			first = false;
		else
			printer.stream() << str;
	}
};
