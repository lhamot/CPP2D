//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "MatchContainer.h"
#include <set>

//! Singleton where all custom printers are registered
class CustomPrinters
{
public:
	static CustomPrinters& getInstance();

	//! A function registering some custom printers
	typedef void(*CustomPrinterRegistrer)(
	  MatchContainer&,
	  clang::ast_matchers::MatchFinder&);

	//! Add a register fonction
	void registerCustomPrinters(CustomPrinterRegistrer registrer);

	//! Get the liste of register functions
	std::set<CustomPrinterRegistrer> const& getRegisterers() const;

private:
	std::set<CustomPrinterRegistrer> registrers;
};

#define REG_CUSTOM_PRINTER(FUNC_NAME) \
	namespace{auto reg = (CustomPrinters::getInstance().registerCustomPrinters(&FUNC_NAME), 0);}
