//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <sstream>
#include <unordered_map>
#include <map>
#include <array>
#include "framework.h"
#include <cmath>
#include "math.h"
#include <iostream>

template<typename T>
struct TestType {};

void check_std_map()
{
	/*std::map<int, int> m1;
	m1[36] = 78;
	CHECK_EQUAL(m1[36], 78);

	auto m2 = m1;
	m2[36] = 581;
	CHECK_EQUAL(m2[36], 581);
	CHECK_EQUAL(m1[36], 78);*/

	//auto lmbd = [](std::map<int, std::string> toto) {};
	//auto lmbd2 = [](std::map<int, int>::value_type toto) {};

	//TestType<std::string> toto;
	//TestType<std::map<std::string, int>> tata;
	std::map<int, std::string> tutu;
	std::map<int, std::string>::value_type titi;
	TestType<TestType<std::map<int, std::string>>> tata;
}

struct ContainHashMap
{
	std::unordered_map<int, int> m1;
};

struct ContainHashMap2
{
	std::unordered_map<int, int> m1;

	ContainHashMap2() = default;

	ContainHashMap2(ContainHashMap2 const& other)
		: m1(other.m1)
	{
	}
};

struct ContainHashMap3
{
	std::unordered_map<int, int> m1;

	ContainHashMap3() = default;

	ContainHashMap3(ContainHashMap3 const& other)
	{
		m1 = other.m1;
	}
};

void check_cmath()
{
	CHECK(std::sqrt(4.) == 2.);
	CHECK(pow(2, 2) == 4);
}

void check_std_unordered_map()
{
	std::unordered_map<int, int> m1;
	m1[36] = 78;
	CHECK(m1[36] == 78);

	std::vector<int> v1;
	std::vector<int> v2;
	for (auto i : m1)
	{
		v1.push_back(i.first);
		v2.push_back(i.second);
	}
	CHECK(v1[0] == 36);
	CHECK(v2[0] == 78);

	{
		ContainHashMap c1;
		c1.m1[15] = 98;
		auto c2 = c1;
		c1.m1[15] = 6;
		CHECK(c2.m1[15] == 98);
		c2 = c1;
		c1.m1[15] = 7;
		CHECK(c2.m1[15] == 6);
	}

	{
		ContainHashMap2 c1;
		c1.m1[15] = 98;
		auto c2 = c1;
		c1.m1[15] = 6;
		CHECK(c2.m1[15] == 98);
		c2 = c1;
		c1.m1[15] = 7;
		CHECK(c2.m1[15] == 6);
	}

	{
		ContainHashMap3 c1;
		c1.m1[15] = 98;
		auto c2 = c1;
		c1.m1[15] = 6;
		CHECK(c2.m1[15] == 98);
		c2 = c1;
		c1.m1[15] = 7;
		CHECK(c2.m1[15] == 6);
	}
}

class Class781
{
public:
	int i = 0;
	Class781() {};
	Class781(int u) :i(u) {};
};

struct Struct781
{
	int i = 0;
	Struct781() = default;
	Struct781(int u) :i(u) {};
};

void check_shared_ptr()
{
	std::shared_ptr<Class781> classptr1;  //default ctor
	CHECK(classptr1 == nullptr);
	classptr1.reset(new Class781());
	CHECK(classptr1 != nullptr);
	std::shared_ptr<Class781> classptr2 = classptr1;  //copy ctor
	CHECK(classptr1 == classptr2);
	//std::shared_ptr<Class781> classptr3(new Class781()); //Use make_shared!!
	//CHECK(classptr3 != nullptr);
	std::shared_ptr<Class781> classptr4 = std::make_shared<Class781>();  //make_shared ctor
	CHECK(classptr4 != nullptr);
	classptr4 = std::make_shared<Class781>(); //assign make_shared
	CHECK(classptr4 != nullptr);
	classptr4 = classptr2; //assign
	CHECK(classptr4 == classptr2);

	std::shared_ptr<Struct781> structptr1; //default ctor
	CHECK(structptr1 == nullptr);
	structptr1 = std::make_shared<Struct781>(); //assign make_shared
	CHECK(structptr1 != nullptr);
	//structptr1.reset(new Struct781()); //Use make_shared!!
	//CHECK(structptr1 != nullptr);
	std::shared_ptr<Struct781> structptr2 = structptr1; //copy stor
	CHECK(structptr2 == structptr1);
	//std::shared_ptr<Struct781> structptr3(new Struct781()); //Use make_shared!!
	//CHECK(structptr2 != nullptr);
	std::shared_ptr<Struct781> structptr4 = std::make_shared<Struct781>(); //make_shared ctor
	CHECK(structptr4 != nullptr);
	structptr4 = structptr1; //assign
	CHECK(structptr4 == structptr1);
	return;
}

void check_unique_ptr()
{
	std::unique_ptr<Class781> classptr1;  //default ctor
	CHECK(classptr1 == nullptr);
	classptr1.reset(new Class781());
	CHECK(classptr1 != nullptr);
	std::unique_ptr<Class781> classptr2 = std::move(classptr1);  //move copy
	CHECK(classptr2 != nullptr);
	CHECK(classptr1 == nullptr);
	//std::unique_ptrd_ptr<Class781> classptr3(new Class781()); //Use make_shared!!
	//CHECK(classptr3 != nullptr);
	std::unique_ptr<Class781> classptr4 = std::make_unique<Class781>();  //make_shared ctor
	(*classptr4).i = 78;
	CHECK_EQUAL((*classptr4).i, 78);
	CHECK(classptr4 != nullptr);
	classptr4 = std::make_unique<Class781>(); //assign make_shared
	CHECK(classptr4 != nullptr);
	classptr4 = std::move(classptr2); //move assign
	CHECK(classptr4 != nullptr);
	CHECK(classptr2 == nullptr);

	std::unique_ptr<Struct781> structptr1; //default ctor
	CHECK(structptr1 == nullptr);
	structptr1 = std::make_unique<Struct781>(); //assign make_shared
	(*structptr1).i = 78;
	CHECK_EQUAL((*structptr1).i, 78);
	CHECK(structptr1 != nullptr);
	//structptr1.reset(new Struct781()); //Use make_shared!!
	//CHECK(structptr1 != nullptr);
	std::unique_ptr<Struct781> structptr2 = std::move(structptr1); //move copy
	CHECK(structptr2 != nullptr);
	CHECK(structptr1 == nullptr);
	//std::unique_ptr<Struct781> structptr3(new Struct781()); //Use make_shared!!
	//CHECK(structptr2 != nullptr);
	std::unique_ptr<Struct781> structptr4 = std::make_unique<Struct781>(); //make_shared ctor
	CHECK(structptr4 != nullptr);
	structptr4 = std::move(structptr2); //assign
	CHECK(structptr4 != nullptr);
	CHECK(structptr2 == nullptr);
	return;
}

void check_stdarray()
{
	std::array<int, 4> a;
	a.fill(2);
	CHECK(a[3] == 2);
	std::array<int, 4> b = {0, 1, 2, 3};
	CHECK(b[1] == 1);
}

void check_stdpair()
{
	std::pair<int, float> p1;
	p1.first = 3;
	p1.second = 3.141f;
	CHECK(p1.first == 3);
	CHECK(p1.second == 3.141f);
	auto p2 = std::pair<int, float>(4, 4.44444f);
	CHECK(p2.first == 4);
	auto p3 = std::make_pair(6, 6.66666f);
	CHECK(p3.first == 6);

	std::get<0>(p1) = 7;
	CHECK(std::get<0>(p1) == 7);
}

class Class783 : public Class781
{
public:
	Class783() { i = 12; }
};

void check_vector()
{
	std::vector<int> a;
	a.push_back(2);
	CHECK_EQUAL(a[0], 2);
	CHECK_EQUAL(static_cast<int>(a.size()), 1);

	enum Blu
	{
		A1,
	};
	std::vector<Blu> b;
	b.push_back(A1);
	CHECK_EQUAL(b[0], A1);
	CHECK_EQUAL(static_cast<int>(b.size()), 1);

	std::vector<std::shared_ptr<Struct781>> c;
	c.push_back(std::make_shared<Struct781>());
	CHECK_EQUAL(c[0]->i, 0);
	CHECK_EQUAL(static_cast<int>(c.size()), 1);

	std::vector<std::shared_ptr<Class781>> d;
	d.push_back(std::make_shared<Class781>());
	CHECK_EQUAL(d[0]->i, 0);
	CHECK_EQUAL(static_cast<int>(d.size()), 1);

	d.push_back(std::make_shared<Class783>());
	CHECK_EQUAL(d[1]->i, 12);
	CHECK_EQUAL(d.at(1)->i, 12);
	CHECK_EQUAL(static_cast<int>(d.size()), 2);

	std::cout << std::endl;
}

void check_tuple()
{
	typedef std::tuple<int, std::map<int, std::string> > sdgdfh;
	typedef std::tuple<int, std::string> Tutu;
	Tutu tutu(42, "azer");
	CHECK_EQUAL(std::get<0>(tutu), 42);
	CHECK_EQUAL(std::get<1>(tutu), "azer");

	typedef std::tuple<int&, std::string&> TutuR;
	//TutuR tutuR(std::get<0>(tutu), std::get<1>(tutu));
	//CHECK_EQUAL(std::get<0>(tutuR), 42);
	//CHECK_EQUAL(std::get<1>(tutuR), "azer");
}

void check_stringstream()
{
	std::stringstream ss;
	ss << "abc" << " " << 42;
	CHECK(ss.str() == "abc 42");
	std::stringstream& ss2 = ss;
	CHECK(ss2.str() == "abc 42");

	std::stringstream ss3("dfgdfgfh");
	CHECK(ss3.str() == "dfgdfgfh");
}

void stdlib_register(TestFrameWork& tf)
{
	auto ts = std::make_unique<TestSuite>();

	ts->addTestCase(check_stringstream);

	ts->addTestCase(check_std_map);

	ts->addTestCase(check_vector);

	ts->addTestCase(check_cmath);

	ts->addTestCase(check_std_unordered_map);

	ts->addTestCase(check_shared_ptr);

	ts->addTestCase(check_unique_ptr);

	ts->addTestCase(check_stdarray);

	ts->addTestCase(check_stdpair);

	ts->addTestCase(check_tuple);

	tf.addTestSuite(std::move(ts));
}