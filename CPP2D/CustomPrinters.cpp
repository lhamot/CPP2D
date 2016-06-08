//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "CustomPrinters.h"

CustomPrinters& CustomPrinters::getInstance()
{
	static CustomPrinters instance;
	return instance;
}

void CustomPrinters::registerCustomPrinters(CustomPrinterRegistrer registrer)
{
	registrers.insert(registrer);
}

std::set<CustomPrinters::CustomPrinterRegistrer> const& CustomPrinters::getRegisterers() const
{
	return registrers;
}
