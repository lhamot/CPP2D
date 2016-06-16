//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>

extern unsigned int testCount;

void check(bool ok, char const* message, int line);
void check_equal(int a, int b, char const* message, int line);

#define CHECK(COND) check(COND, #COND, __LINE__) //if(!(COND)) error(#COND)
#define CHECK_EQUAL(A, B) check_equal(A, B, #A " == " #B, __LINE__) //if(!(COND)) error(#COND)

typedef void(*TestCase)(); //!< One small test independant of others

//! Store some related test cases, and run them
class TestSuite
{
	TestSuite(TestSuite const&) = delete;
	TestSuite& operator=(TestSuite const&) = delete;

	std::vector<TestCase> testCases;

public:
	TestSuite() = default;

	//! Run all test cases
	void run() const noexcept;

	//! @brief Add a test cases
	//! @pre TestCase != nullptr
	void addTestCase(TestCase testCase);
};

//! Store all test suites, and run them
class TestFrameWork
{
	TestFrameWork(TestFrameWork const&) = delete;
	TestFrameWork& operator=(TestFrameWork const&) = delete;

	std::vector<std::unique_ptr<TestSuite const> > testSuites;

public:
	TestFrameWork() = default;

	//! Run all test suites
	void run() const noexcept;

	//! @brief Print a report to std output
	//! @pre TestFrameWork::run has been called
	void print_results();

	//! @brief Add a test suite
	//! @pre testSuite != nullptr
	void addTestSuite(std::unique_ptr<TestSuite>&& testSuite);
};

