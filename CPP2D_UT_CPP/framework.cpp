//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <cstdio>
#include "framework.h"

unsigned int testCount = 0;

void check(bool ok, char const* message, int line)
{
	++testCount;
	if (!ok)
	{
		//printf(message);
		printf("%s    ->Failed at line %u\n", message, line);
	}
}

void print_results()
{
	printf("%u tests\n", testCount);
}