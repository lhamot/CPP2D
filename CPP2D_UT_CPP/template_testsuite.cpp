//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "framework.h"
#include <iostream>
#include <algorithm>

template<typename T>
auto sum(T value)
{
	return value;
}

template<typename T, typename... Args>
auto sum(T const& value, Args const&... args)
{
	return value + sum(args...);
}


void check_variadic_tmpl()
{
	CHECK(sum(1, 0.5f, 0.25) == 1.75);
}

struct S
{
	int val;
};

static S StatStruct = { 42 };

S const& ret_ref()
{
	return StatStruct;
}

template<typename T>
S const& ret_ref_tmpl()
{
	return StatStruct;
}

template<typename T>
void check_return_ref_tmpl()
{
	auto const& a = ret_ref_tmpl<T>();
	CHECK_EQUAL(a.val, 43);
	StatStruct.val = 44;
	CHECK_EQUAL(a.val, 44);

	CHECK(std::min(2, 3) == 2);
	CHECK(std::min(T(2), T(3)) == 2);
}

void check_return_ref()
{
	auto const& a = ret_ref();
	CHECK_EQUAL(a.val, 42);
	StatStruct.val = 43;
	CHECK_EQUAL(a.val, 43);
}

struct QA
{
	struct B
	{

	};
};

template<typename T>
struct QC;

template<>
struct QC<QA::B>
{
	static const int U = 18;
};

void check_qualifier()
{
	CHECK_EQUAL(QC<QA::B>::U, 18);
}


void template_register(TestFrameWork& tf)
{
	auto ts = std::make_unique<TestSuite>();

	ts->addTestCase(check_variadic_tmpl);

	ts->addTestCase(check_return_ref);

	ts->addTestCase(check_qualifier);

	ts->addTestCase(check_return_ref_tmpl<char>);

	tf.addTestSuite(std::move(ts));
}