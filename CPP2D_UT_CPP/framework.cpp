//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <cstdio>
#include "framework.h"

unsigned int testCount = 0;

void check(bool ok, char const* message, int line, char const* file)
{
	++testCount;
	if (!ok)
	{
		//printf(message);
		printf("%s    ->Failed at line %u, in file %s\n", message, line, file);
	}
}

void TestSuite::run() const noexcept
{
	for (TestCase const& testcase : testCases)
	{
		try
		{
			testcase();
		}
		catch (std::exception& ex)
		{
			printf("%s", ex.what());
		}
	}
}

void TestSuite::addTestCase(TestCase testCase)
{
	testCases.push_back(testCase);
}

void TestFrameWork::run() const noexcept
{
	for (std::unique_ptr<TestSuite const> const& suite : testSuites)
		suite->run();
}

void TestFrameWork::print_results()
{
	printf("%u tests\n", testCount);
}

void TestFrameWork::addTestSuite(std::unique_ptr<TestSuite>&& testSuite)
{
	testSuites.emplace_back(std::move(testSuite));
}

