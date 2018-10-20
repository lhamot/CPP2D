//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "framework.h"
#include "test.h"
#include "stdlib_testsuite.h"
#include "template_testsuite.h"

int main(
	int argc, char** argv
)
{
	int argvCount = argc;
	CHECK(argvCount > 0);
	CHECK(argv[0] != nullptr);

	TestFrameWork testFrameWork;
	test_register(testFrameWork); 
	stdlib_register(testFrameWork);
	template_register(testFrameWork);
	testFrameWork.run();

	testFrameWork.print_results();

	return 0;
}