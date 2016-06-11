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

void template_register(TestFrameWork& tf)
{
	auto ts = std::make_unique<TestSuite>();

	ts->addTestCase(check_variadic_tmpl);

	tf.addTestSuite(std::move(ts));
}