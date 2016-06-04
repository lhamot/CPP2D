//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "framework.h"
#include "test.h"

int main()
{
	TestFrameWork testFrameWork;
	test_register(testFrameWork); 
	testFrameWork.run();

	testFrameWork.print_results();

	return 0;
}