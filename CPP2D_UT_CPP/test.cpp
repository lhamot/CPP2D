//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "test.h"
#include "framework.h"
#include <cstdarg>
#include <cstdio>
#include <utility>
#include <stdexcept>
#include <array>
#include <unordered_map>
#include <cassert>
#include <vector>
#include <memory>
#include <cstring>


class noncopyable
{
	noncopyable(noncopyable const&);
	noncopyable& operator=(noncopyable const&);
public:
	noncopyable() = default;
};

class Tata : noncopyable
{
	virtual int funcA2() = 0;

public:
	virtual ~Tata() = default;
	virtual int funcV()
	{
		return 1;
	}

	int funcF()
	{
		return 1;
	}

	virtual int funcA() = 0;
};

class Tata2 : public Tata
{
public:
	int value;

	Tata2(int arg = 13)
	{
		value = arg;
	}

	int funcV() override
	{
		return 2;
	}

	int funcF(int i = 0)
	{
		return 2;
	}

	int funcA() override
	{
		return 4;
	}

	int funcA2() override
	{
		return 5;
	}

	Tata2* getThis()
	{
		return this;
	}

	Tata2& getThisRef()
	{
		return *this;
	}
};

template<int I>
class TmplClass1
{
public:
	static int const value = I;
};

template<int I>
class TmplClass2 : public TmplClass1<I> {};

template<>
class TmplClass2<0> : public TmplClass1<42> {};

template<typename T1, typename T2>
bool fl_equal(T1 v1, T2 v2)
{
	int argc = 2;
	auto diff = ((v1 - v2) + argc) - argc;
	return (diff < 0. ? -diff : diff) < 0.00001;
}

void check_operators()
{
	//**************************** operators **********************************
	//    Math
	CHECK(45 == 45);
	CHECK(11 != 88);
	CHECK(3 + 2 == 5);
	CHECK(3 - 2 == 1);
	CHECK(3 * 2 == 6);
	CHECK(6 / 2 == 3);

	int u = 0;
	u += 2;
	CHECK(u == 2);
	u -= 1;
	CHECK(u == 1);
	u *= 6;
	CHECK(u == 6);
	u /= 3;
	CHECK(u == 2);

	//    Bin
	CHECK(true && true);
	CHECK(true || false);

	// bitwise
	CHECK((0x3 & 0x1) == 0x1);
	CHECK((0x3 | 0x1) == 0x3);
	CHECK((0x3 ^ 0x1) == 0x2);
}

int func0() { return 3; }

int func10(int i = 42) { return i; }

void func12(int& i, int u) { i = 42; u = 43; }

int z = 45;

int& func15() { return z; }

const int& func16() { return z; }

void check_function()
{
	//*********************** Functions ***************************************
	CHECK(func0() == 3);

	CHECK(func10(7) == 7);

	CHECK(func10() == 42);

	int a = 0, b = 0;
	func12(a, b);
	CHECK(a == 42 && b == 0);

	func15() = 98;
	CHECK(z == 98);
	CHECK(&func15() == &z);
}

void check_static_array()
{
	int tab[3] = { 1, 2, 3 };
	tab[1] = 42;
	CHECK(tab[1] == 42);
	CHECK(tab[2] == 3);

	int tab3[] = { 4, 5, 6, 7 };
	CHECK(sizeof(tab3) == 4 * sizeof(int));

	char const* tabStr[] = {"aa", "bb", "cc"};
	CHECK(sizeof(tabStr) == 3 * sizeof(char const*));

	int tabA[3] = { 1, 2, 3 };
	int tabB[3] = { 1, 2, 3 };
	CHECK(tabA != tabB);  // Pointer comparison

	typedef int start[];

	start bli = { 4, 8, 9 };
	CHECK(bli[2] == 9);
}

//! To ensure the same function isn't print twice
void check_dynamic_array();

void check_dynamic_array()
{
	int *tab2 = new int[46];
	tab2[6] = 42;
	CHECK(tab2[6] == 42);
	delete[] tab2;
	tab2 = nullptr;

	int *tabC = new int[3];
	int *tabD = new int[3];
	for (int i = 0; i < 3; ++i)
	{
		tabC[i] = i - 1;
		tabD[i] = i - 1;
	}
	CHECK(tabD != tabC); // Pointer comparison

	tabD = tabC;   // Copy pointer
	tabC[1] = 78;
	CHECK(tabD == tabC); // Pointer comparison
}

struct Toto3
{
	int a;
	float b;
	bool c;
	static short u;

	int getA() const
	{
		return a;
	}

	void setA(int a_)
	{
		a = a_;
	}

	static int getStatic()
	{
		return 12;
	}

	bool operator==(Toto3 const& other) const
	{
		return a == other.a && b == other.b && c == other.c;
	}

	//virtual void impossible(){}
};

short Toto3::u = 36;

struct MultiplArgsCtorStruct
{
	int a;
	float b; //!< comment b
	bool c;
	MultiplArgsCtorStruct(int a_,
		float b_,
		bool c_ //!< comment c
		)
		: a(a_)
		, b(b_)
		, c(c_)
	{
	}
};

template<typename T>
struct Toto4
{
	T tutu;
	Toto4(int a) :tutu(a, 3.3f, true) {}
};

//! To ensure the same function isn't print twice
extern "C" {
	int a_func_with_c_export(int a, int b, int c);
	// Blablabla
}

// Blablabla2
int a_func_with_c_export(int a, int b, int c)
{
	return a + b + c;
}

void check_struct()
{
	//! comment Toto
	struct Toto
	{
		int a;
		float b;
		bool c;
	};
	Toto toto; //defult ctor
	toto.a = 6;
	// test commment
	CHECK(toto.a == 6);

	Toto toto2 = { 7, 8.99f, true };
	CHECK(toto2.a == 7 && fl_equal(toto2.b, 8.99) && toto2.c == true);

	//! comment Toto2
	struct Toto2
	{
		//! comment a
		int a;
		float b; //!< comment b
		bool c;
		Toto2(int a_,
			float b_,
			bool c_ //!< comment c
			)
			: a(a_)
			, b(b_)
			, c(c_)
		{
		}

		Toto2* getThis()
		{
			return this;
		}

		Toto2& getThisRef()
		{
			return *this;
		}
	};

	Toto2 toto3(18, 12.57f, true);
	CHECK(toto3.a == 18 && fl_equal(toto3.b, 12.57) && toto3.c == true);
	CHECK(toto3.getThis() == &toto3);
	CHECK(&toto3.getThisRef() == &toto3);

	Toto3 t3;
	t3.setA(16);
	CHECK(t3.getA() == 16);
	CHECK(Toto3::getStatic() == 12);

	Toto3 const t4 = { 16 };
	CHECK(t4.getA() == 16);

	Toto3::u = 17;
	CHECK(Toto3::u == 17);

	t3.b = 0.00795f;
	Toto3 t5 = t3;
	CHECK(&t5 != &t3);  // Pointer compatison
	CHECK(t5 == t3);   //Content comparison

	Toto4<MultiplArgsCtorStruct> tt4(1);
	CHECK(tt4.tutu.b == 3.3f);

	struct Toto5
	{
		Toto2 tutu;
		Toto5(int a) :tutu(a, 3.3f, true) {}
	};
	Toto5 tt5(1);
	CHECK(tt5.tutu.b == 3.3f);

	typedef struct {int x; } c_style_struct_dect;
	c_style_struct_dect i;
	i.x = 12;
	CHECK_EQUAL(i.x, 12);
};

// Template type
template<typename T, int I>
T funcT()
{
	return T(I);
}

// Template type deduction
template<typename T>
T funcT2(T a, int mult)
{
	return a * mult;
}

template<typename T1, typename T2>
struct is_same
{
	static bool const value = false;
};

template<typename T1>
struct is_same<T1, T1>
{
	static bool const value = true;
};

void check_template_struct_specialization()
{
	bool b = is_same<int, int>::value;
	CHECK(b);
	b = is_same<int, float>::value;
	CHECK(!b);
}

void check_template_function()
{
	auto res1 = funcT<int, 4>();
	const bool isInt = is_same<decltype(res1), int>::value;
	CHECK(isInt);
	CHECK(res1 == 4);
	auto res2 = funcT2(3, 2);
	CHECK(res2 == 6);
}


void check_class()
{
	Tata2* t2 = new Tata2();
	CHECK(t2->funcV() == 2);
	CHECK(t2->funcF() == 2);
	CHECK(t2->funcA() == 4);
	CHECK(t2->funcA2() == 5);
	Tata* t = t2;
	CHECK(t->funcV() == 2);
	CHECK(t->funcF() == 1);
	CHECK(t2->getThis() == t2);
	CHECK(t2 == t);
	CHECK(&t2->getThisRef() == t2);
	delete t;
	t = nullptr;
	CHECK(t == nullptr);

	CHECK(TmplClass2<3>::value == 3);
	CHECK(TmplClass2<0>::value == 42);

	Tata2 t3;
	CHECK(&t3 != nullptr);
	CHECK(t3.value == 13);

	Tata2 t4(54);
	CHECK(&t4 != nullptr);
	CHECK(t4.value == 54);
}

template<typename A, typename B>
struct same_type;

template<typename A>
struct same_type<A, A> { static bool const value = true; };

template <typename T> struct A { typedef T type; };
template <typename T> using Type = typename A<T>::type;

void check_type_alias()
{
	using type = int;
	static_assert(same_type<type, int>::value, "same_type<type, int>::value");

	static_assert(same_type<Type<int>, A<int>::type>::value, "same_type<type, int>::value");
}

struct HasBitField
{
	unsigned char a : 1, b : 2, c : 3;
};

struct HasBitField2
{
	int a : 7;
	int b : 8;
	int c;
};

void check_bitfield()
{
	static_assert(sizeof(HasBitField) == 1, "sizeof(HasBitField) == 1");
	static_assert(sizeof(HasBitField2) == 8, "sizeof(HasBitField2) == 8");

	HasBitField bf; // = { 0, 1, 3 }; // Do not work in D
	bf.a = 0;
	bf.b = 1;
	bf.c = 3;
	CHECK(bf.a == 0 && bf.b == 1 && bf.c == 3);
	bf.a = 1;
	CHECK(bf.a == 1 && bf.b == 1 && bf.c == 3);
	bf.b = 2;
	CHECK(bf.a == 1 && bf.b == 2 && bf.c == 3);
	bf.c = 2;
	CHECK(bf.a == 1 && bf.b == 2 && bf.c == 2);
}

int decayed_type_func(int a[2], int b[], int(c)(int))
{
	return c(a[0] + a[1] + b[3]);
}

int mult2(int val)
{
	return val * 2;
}

void check_decayed_type()
{
	int a[2] = { 1, 2 };
	int b[4] = { 0, 1, 2, 4 };
	CHECK(decayed_type_func(a, b, mult2) == 14);
}

template <typename T> struct InjectedClassNameType
{
public:
	T data = 2;
	InjectedClassNameType const& toto(InjectedClassNameType const* a)
	{
		return *a;
	}
};

void check_injectedclassnametype()
{
	InjectedClassNameType<int const> toto;
	InjectedClassNameType<int const> toto2;
	CHECK(&toto.toto(&toto2) == &toto2);
}


class ConvertibleClass
{
	int a_;
	void meth(){} //check private non virtual method
public:
	ConvertibleClass(int a) :a_(a) {}

	operator int()
	{
		return a_;
	}
};

struct ConvertibleStruct
{
	int a_;

	ConvertibleStruct(int a) :a_(a) {}

	operator int()
	{
		return a_;
	}
};

void check_convertion_operator()
{
	ConvertibleClass a(7);
	CHECK(a == 7);

	ConvertibleStruct b(19);
	CHECK(b == 19);
}

class AbsMethClass
{
public:
	virtual ~AbsMethClass() {};
	virtual int meth() = 0;
};

class AbsMethClass2 : public AbsMethClass
{
	int meth()
	{
		return 12;
	}
};

void check_abstract_keyword()
{
	AbsMethClass* a = new AbsMethClass2();
	CHECK(a->meth() == 12);
	delete a;
}


void check_struct_opassign()
{
	struct S
	{
		int a;
		int b;

		S& operator = (S const& other)
		{
			a = other.a;
			return *this;
		}
	};

	S a = { 1, 2 };
	S const b = { 3, 4 };
	a = b;
	CHECK(a.a == 3 && a.b == 2);
}

void check_class_opassign()
{
	class S
	{
	public:
		int a;
		int b;
		S(int a_, int b_) :a(a_), b(b_) {}

		S& operator = (int c)
		{
			a = c;
			return *this;
		}
	};

	S a(1, 2);
	a = 59;
	CHECK(a.a == 59 && a.b == 2);

	S *c = new S(1, 2);
	*c = 47;
	CHECK(c->a == 47);
	CHECK(c->b == 2);
	delete c;
}


template<typename A>
bool is_int()
{
	return false;
}

template<>
bool is_int<int>()
{
	return true;
}

template<typename A>
bool is_float();

template<typename A>
bool is_float()
{
	return false;
}

template<>
bool is_float<float>();

template<>
bool is_float<float>()
{
	return true;
}

void check_tmpl_func_spec()
{
	struct Blip {};
	CHECK(is_int<Blip>() == false);
	CHECK(is_int<int>() == true);
	CHECK(is_float<float>() == true);
}

void var_func(char* buffer, const char* fmt, ...)
{
	//! TODO : Waiting for macro handling
	/*va_list args;
	va_start(args, fmt);

	while (*fmt != '\0') {
		if (*fmt == 'd') {
			int i = va_arg(args, int);
			buffer += sprintf(buffer, "%d ", i);
		}
		else if (*fmt == 'c') {
			// note automatic conversion to integral type
			int c = va_arg(args, int);
			buffer += sprintf(buffer, "%c ", c);
		}
		else if (*fmt == 'f') {
			double d = va_arg(args, double);
			buffer += sprintf(buffer, "%f ", d);
		}
		++fmt;
	}

	va_end(args);*/
	return;
}


void check_variadic()
{
	char buffer[1000];
	var_func(buffer, "dcff", 3, 'a', 1.999, 42.5);
}

template<int I>
struct TmplMethstruct2
{
	int a;
};


template<int I>
struct TmplMethstruct
{
	int a = I;

	TmplMethstruct() = default;

	template<int I2>
	TmplMethstruct(TmplMethstruct<I2> const&) : a(I2) {}
	
	template<int I2>
	operator TmplMethstruct2<I2>()
	{
		TmplMethstruct2<I2> bli = {a};
		return bli;
	}

	template<int I2>
	TmplMethstruct& operator = (TmplMethstruct<I2> const& ot)
	{
		a = ot.a;
		return *this;
	}

	template<int I2>
	TmplMethstruct operator - (TmplMethstruct<I2> const& ot)
	{
		TmplMethstruct res;
		res.a = a - ot.a;
		return res;
	}

	template<int I2>
	bool operator < (TmplMethstruct<I2> const& ot)
	{
		return a < ot.a;
	}

};

void check_tmpl_meth()
{
	TmplMethstruct<4> a;
	TmplMethstruct<7> b(a);
	CHECK(a.a == 4); //default ctor
	CHECK(b.a == 4); //copy ctor
	b.a = 56;
	b = a;  // copy operator
	CHECK(b.a == 4);
	b.a = 56;

	TmplMethstruct2<78> c = b; //cast operator
	CHECK(c.a == 56);

	auto d = b - a;
	CHECK(d.a == 52); // binary operator

	//CHECK(a < d); // comparison
}

int tata(short a) { return a * 2; }

int toto(short a) { return a * 4; }

void check_function_pointer()
{
	int(*a)(short) = tata;
	int(*b)(short) = toto;
	CHECK(a != b);
	CHECK(2 * a(2) == b(2));
	a = b;
	CHECK(a == b);
}


void check_method_pointer()
{
	struct MethPointerStruct
	{
		int tata(short a) { return a * 2; }
		int toto(short a) { return a * 2; }
	};
	/*MethPointerStruct toto;
	int (MethPointerStruct::*a)(short) = &MethPointerStruct::tata;
	int (MethPointerStruct::*b)(short) = &MethPointerStruct::toto;
	CHECK(a != b);
	CHECK((toto.*a)(2) == (toto.*b)(2));
	a = b;
	CHECK(a == b);
	*/
	return;
}


void check_member_pointer()
{
	struct MemPointerStruct
	{
		int a = 1;
		int b = 2;
	} instance;

	/*int MemPointerStruct::* i = &MemPointerStruct::a;
	int MemPointerStruct::* j = &MemPointerStruct::b;
	CHECK(i != j);
	
	j = &MemPointerStruct::b;
	CHECK((instance.*i) == 1);
	CHECK((instance.*j) == 2);
	i = j;
	CHECK(i == j);
	CHECK((instance.*i) == 2);*/
	return;
}

enum Empty {};

struct FriendStruct1;
struct FriendStruct2
{
	friend FriendStruct1;
};
struct FriendStruct1
{
	friend FriendStruct2;
};


struct CheckUDAArg
{
	typedef int A;
	static int const a = 42;
};

template<typename T>
struct CheckUDA
{
	typedef typename T::A T_A;
	T_A a = T::a;
	T_A func(T_A a = T::a)
	{
		return a;
	};
};

void check_uninstantiated_default_arg()
{
	CheckUDA<CheckUDAArg> a;
	CHECK(a.a == 42);
	CHECK(a.func() == 42);
}

void check_class_instantiation()
{
	class StackClass
	{
	public:
		int a;

		StackClass(int a_ = 0) : a(a_) {}
	};
	StackClass a;
	StackClass b(1);
	StackClass c = 2;

	StackClass *d = new StackClass;
	StackClass *e = new StackClass(1);
	CHECK(d->a != e->a);
	CHECK(d != e);
	delete d;

	d = e;
	CHECK(d == e);
	delete e;
}

typedef int incomp_arr[];
int func_incomp_arr(incomp_arr arr)
{
	return arr[3];
}

void check_incomplete_array_type()
{
	int toto[] = { 0, 1, 2, 3 };
	CHECK(func_incomp_arr(toto) == 3);
}


template<typename T>
struct UninstantiatedStruct
{
	template<typename I>
	typename T::A func(typename I::B a)
	{
		typename I::C c;
		typename I::D d;
		return c + d.z - I::C::y;
	}
};

void check_builtin_macro()
{
	auto l = __LINE__;
	CHECK(strstr(__FILE__, "test") != nullptr);
	CHECK(strstr(__FUNCTION__, "check_builtin_macro") != nullptr);
	CHECK(strstr(__func__, "check_builtin_macro") != nullptr);
	CHECK(__LINE__ == l + 4);
	assert(2 * 3 == 6);
}

void check_incr_pointer()
{
	size_t const SIZE = 10;
	int tab[SIZE];
	int val = 0;
	for (int* ptr = tab; ptr != tab + 10; ++ptr, ++val) // preincr
		*ptr = val;
	int* ptr = tab;
	ptr++;					//postincr
	ptr += 2;				// +=
	int* ptr2 = ptr + 2;	//+
	CHECK(*ptr2 == 5);
}

template<typename A, typename B, typename C, typename D>
auto comma4(A const& a, B const& b, C const& c, D const& d)
{
	return d;
}

#define UT_MACRO(expr1, expr2, expr3)  \
	comma4(var##expr1 = 42, var2 = #expr2, var3 = expr3, expr1 + expr3)

#define UT_MACRO_STMT  \
int a = 1; \
int b = 2;

#define UT_MACRO_STMT_CLASS(P, T, N, V) P: T N = V;

class MacroClass
{
	UT_MACRO_STMT_CLASS(public, int, a, 1);
	UT_MACRO_STMT_CLASS(public, int, b, 2);
};

template<int I>
struct MPInt {};

int forward(int i)
{
	return i;
}

#define UT_MACRO_EXPR(A, B) A + B

void check_function_macro()
{
	int var1(0);
	char const* var2 = nullptr;
	int var3 = 0;
	int var4(UT_MACRO(1, 2, 3)); 

	CHECK(var1 == 42);
	CHECK(var2[0] == '2');
	CHECK(var3 == 3);
	CHECK(var4 == 4);

	UT_MACRO_STMT

	CHECK(a == 1 && b == 2);

	MacroClass* m = new MacroClass();
	CHECK(m->a == 1 && m->b == 2);
	delete m;

	CHECK(forward(UT_MACRO_EXPR(4, 3)) == 7);
}

void check_range_based_for_loop()
{
	int tab[] = { 1, 2, 3 };
	int sum = 0;
	for (int i : tab)
		sum += i;
	CHECK(sum == 6);
	
	sum = 0;
	for (auto i : tab)
		sum += i;
	CHECK(sum == 6);

	for (int& i : tab)
		i = i + 1;
	CHECK_EQUAL(tab[0], 2);
	CHECK_EQUAL(tab[1], 3);
	CHECK_EQUAL(tab[2], 4);

	for (auto& i : tab)
		i = i + 1;
	CHECK(tab[0] == 3 && tab[1] == 4 && tab[2] == 5);

	int ctab[] = { 1, 2, 3 };
	sum = 0;
	for (auto const& i : ctab)
		sum += i;
	CHECK(sum == 6);

}

struct OverloadOpStuct2
{
	int value;
};

struct OverloadOpStuct
{
	int value = 0;

	explicit OverloadOpStuct(int v) : value(v) {};

	// Unary operators
	OverloadOpStuct operator -() const { return OverloadOpStuct(-value); }
	OverloadOpStuct operator +() const { return OverloadOpStuct(+value); }
	OverloadOpStuct operator *() const { return OverloadOpStuct(value * value); }
	OverloadOpStuct& operator ++()
	{
		++value;
		return *this;
	}
	OverloadOpStuct& operator --()
	{
		--value;
		return *this;
	}
	OverloadOpStuct operator ++(int)
	{
		OverloadOpStuct tmp(*this);
		++(*this);
		return tmp;
	}
	OverloadOpStuct operator --(int)
	{
		OverloadOpStuct tmp(*this);
		--(*this);
		return tmp;
	}

	// Cast operator
	operator int() const {return 42;}

	//*************** Binary operators ************************************************************

	// Arithmetic operators

	OverloadOpStuct& operator += (OverloadOpStuct const& other)
	{
		value += other.value;
		return *this;
	}

	OverloadOpStuct operator + (OverloadOpStuct const& other) const
	{
		OverloadOpStuct tmp(*this);
		return tmp += other;
	}

	OverloadOpStuct& operator -= (OverloadOpStuct const& other)
	{
		value -= other.value;
		return *this;
	}

	OverloadOpStuct operator - (OverloadOpStuct const& other) const
	{
		OverloadOpStuct tmp(*this);
		return tmp -= other;
	}

	OverloadOpStuct& operator *= (OverloadOpStuct const& other)
	{
		value *= other.value;
		return *this;
	}

	OverloadOpStuct operator * (OverloadOpStuct const& other) const
	{
		OverloadOpStuct tmp(*this);
		return tmp *= other;
	}

	OverloadOpStuct& operator /= (OverloadOpStuct const& other)
	{
		value /= other.value;
		return *this;
	}

	OverloadOpStuct operator / (OverloadOpStuct const& other) const
	{
		OverloadOpStuct tmp(*this);
		return tmp /= other;
	}

	// relational operators

	bool operator == (OverloadOpStuct const& other) const
	{
		return value == other.value;
	}

	bool operator != (OverloadOpStuct const& other) const
	{
		return !(*this == other);
	}

	bool operator < (OverloadOpStuct const& other) const
	{
		return (this->value < other.value);
	}

	bool operator > (OverloadOpStuct const& other) const
	{
		return (this->value > other.value);
	}

	bool operator <= (OverloadOpStuct const& other) const
	{
		return (*this < other) || (*this == other);
	}

	bool operator >= (OverloadOpStuct const& other) const
	{
		return (this->value >= other.value);
	}

	// asymetric relational operators
	bool operator == (OverloadOpStuct2 const& other) const
	{
		return value == other.value;
	}

	bool operator != (OverloadOpStuct2 const& other) const
	{
		return !(*this == other);
	}

	bool operator < (OverloadOpStuct2 const& other) const
	{
		return (this->value < other.value);
	}

	bool operator > (OverloadOpStuct2 const& other) const
	{
		return (this->value > other.value);
	}

	bool operator <= (OverloadOpStuct2 const& other) const
	{
		return (*this < other) || (*this == other);
	}

	bool operator >= (OverloadOpStuct2 const& other) const
	{
		return (this->value >= other.value);
	}

	//************************ logical operators **************************************************
	bool operator ! () const
	{
		return this->value == 0;
	}

	bool operator && (OverloadOpStuct const& other) const
	{
		return (!!*this) && (!!other);
	}

	bool operator || (OverloadOpStuct const& other) const
	{
		return (!!*this) || (!!other);
	}

	//************************ Bitwise operators **************************************************
	OverloadOpStuct operator ~ () const
	{
		return OverloadOpStuct(~(this->value));
	}

	OverloadOpStuct operator & (OverloadOpStuct const& other) const
	{
		return OverloadOpStuct(this->value & other.value);
	}

	OverloadOpStuct operator | (OverloadOpStuct const& other) const
	{
		return OverloadOpStuct(this->value | other.value);
	}

	OverloadOpStuct operator ^ (OverloadOpStuct const& other) const
	{
		return OverloadOpStuct(this->value ^ other.value);
	}

	OverloadOpStuct operator << (int count) const
	{
		return OverloadOpStuct(this->value << count);
	}

	OverloadOpStuct operator >> (int count) const
	{
		return OverloadOpStuct(this->value >> count);
	}

	// Member and pointer operators
	int operator [] (int arg) const
	{
		return value + arg;
	}

	int const* operator & () const
	{
		return &value;
	}

	int* operator & ()
	{
		return &value;
	}

	OverloadOpStuct* operator -> ()
	{
		return this;
	}

	int operator()(int a, int b)
	{
		return a + b;
	}

	int operator,(int a)
	{
		return a * 3;
	}
};

void check_overloaded_operator()
{
	//****************   Unary operators  *********************************************************
	{
		OverloadOpStuct i(18);
		CHECK((-i).value == -18);
		CHECK((+i).value == +18);
		CHECK((~i).value == ~18);
		CHECK((*i).value == 18 * 18);
		// Pre incr and decr
		CHECK((++i).value == 19);
		CHECK(i.value == 19);
		CHECK((--i).value == 18);
		CHECK(i.value == 18);
		// post incr and decr
		CHECK((i++).value == 18);
		CHECK(i.value == 19);
		CHECK((i--).value == 19);
		CHECK(i.value == 18);
	}

	//*************************** Cast operator ***************************************************
	{
		OverloadOpStuct i(18);
		CHECK((int)i == 42);
	}

	//********************* Binary operators ******************************************************
	// Arithmetic operators
	{
		OverloadOpStuct a(6);
		OverloadOpStuct b(3);

		OverloadOpStuct mu = a * b;
		CHECK(mu.value == 18);

		OverloadOpStuct d = a / b;
		CHECK(d.value == 2);

		OverloadOpStuct p = a + b;
		CHECK(p.value == 9);

		OverloadOpStuct mi = a - b;
		CHECK(mi.value == 3);

		mu /= b;
		CHECK(mu.value == a.value);

		d *= b;
		CHECK(d.value == a.value);

		p -= b;
		CHECK(p.value == a.value);

		mi += b;
		CHECK(mi.value == a.value);
	}

	// relational operators
	{
		OverloadOpStuct a6(6);
		OverloadOpStuct b6(6);
		OverloadOpStuct c3(3);
		CHECK(b6 == a6);
		CHECK(b6 <= a6);
		CHECK(b6 >= a6);
		CHECK(c3 != a6);
		CHECK(c3  < a6);
		CHECK(c3 <= a6);
		CHECK(a6  > c3);
		CHECK(a6 >= c3);
	}

	// Asymetric relational operators
	{
		OverloadOpStuct2 a6 = { 6 };
		OverloadOpStuct b6(6);
		OverloadOpStuct c3(3);
		OverloadOpStuct d9(9);
		CHECK(b6 == a6);
		CHECK(b6 <= a6);
		CHECK(b6 >= a6);
		CHECK(c3 != a6);
		CHECK(c3  < a6);
		CHECK(c3 <= a6);
		CHECK(d9  > a6);
		CHECK(d9 >= a6);
	}

	// Logical operators
	{
		OverloadOpStuct a0(0);
		OverloadOpStuct b0(0);
		OverloadOpStuct c1(48);
		OverloadOpStuct d1(79);
		CHECK(!a0);
		CHECK(!!c1);
		CHECK(!(a0 && b0));
		CHECK(!(a0 && c1));
		CHECK(!(c1 && a0));
		CHECK(c1 && d1);
		CHECK(!(a0 || b0));
		CHECK(a0 || c1);
		CHECK(c1 || a0);
		CHECK(c1 || d1);
	}

	//************************ Bitwise operators **************************************************
	{
		auto inv = ~decltype(OverloadOpStuct::value)(0);
		OverloadOpStuct a000(0);
		OverloadOpStuct a110(6);
		OverloadOpStuct a011(3);
		CHECK((~a000).value == inv);
		CHECK((a110 & a011) == OverloadOpStuct(2));
		CHECK((a110 | a011) == OverloadOpStuct(7));
		CHECK((a110 ^ a011) == OverloadOpStuct(5));
		CHECK((a110 >> 1) == OverloadOpStuct(3));
		CHECK((a110 << 1) == OverloadOpStuct(12));
	}

	// Member and pointer operators
	{
		OverloadOpStuct a13(13);
		CHECK(a13[7] == 20);
		//CHECK(&a13 == &a13.value);
		CHECK(a13->value == a13.value);
	}

	//********************************* Other operators *******************************************
	{
		OverloadOpStuct a13(13);
		CHECK(a13(1, 2) == 3);
		//CHECK((a13, 18) == (18 * 3)); //Can't overload , in D
	}
}


struct ExtOverloadOpStuct
{
	int value = 0;

	explicit ExtOverloadOpStuct(int v) : value(v) {};
};

// Unary operators
ExtOverloadOpStuct operator -(ExtOverloadOpStuct const& self) { return ExtOverloadOpStuct(-self.value); }
ExtOverloadOpStuct operator +(ExtOverloadOpStuct const& self) { return ExtOverloadOpStuct(+self.value); }
ExtOverloadOpStuct operator *(ExtOverloadOpStuct const& self) { return ExtOverloadOpStuct(self.value * self.value); }
ExtOverloadOpStuct& operator ++(ExtOverloadOpStuct& self)
{
	++self.value;
	return self;
}
ExtOverloadOpStuct& operator --(ExtOverloadOpStuct& self)
{
	--self.value;
	return self;
}
ExtOverloadOpStuct operator ++(ExtOverloadOpStuct& self, int)
{
	ExtOverloadOpStuct tmp(self);
	++self;
	return tmp;
}
ExtOverloadOpStuct operator --(ExtOverloadOpStuct& self, int)
{
	ExtOverloadOpStuct tmp(self);
	--self;
	return tmp;
}

//*************** Binary operators ************************************************************

// Arithmetic operators

ExtOverloadOpStuct& operator += (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	self.value += other.value;
	return self;
}

ExtOverloadOpStuct operator + (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	ExtOverloadOpStuct tmp(self);
	return tmp += other;
}

ExtOverloadOpStuct& operator -= (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	self.value -= other.value;
	return self;
}

ExtOverloadOpStuct operator - (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	ExtOverloadOpStuct tmp(self);
	return tmp -= other;
}

ExtOverloadOpStuct& operator *= (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	self.value *= other.value;
	return self;
}

ExtOverloadOpStuct operator * (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	ExtOverloadOpStuct tmp(self);
	return tmp *= other;
}

ExtOverloadOpStuct& operator /= (ExtOverloadOpStuct& self, ExtOverloadOpStuct const& other)
{
	self.value /= other.value;
	return self;
}

ExtOverloadOpStuct operator / (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	ExtOverloadOpStuct tmp(self);
	return tmp /= other;
}

// relational operators

bool operator == (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return self.value == other.value;
}

bool operator != (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return !(self == other);
}

bool operator < (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (self.value < other.value);
}

bool operator > (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (self.value > other.value);
}

bool operator <= (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (self < other) || (self == other);
}

bool operator >= (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (self.value >= other.value);
}

// asymetric relational operators
bool operator == (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return self.value == other.value;
}

bool operator != (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return !(self == other);
}

bool operator < (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return (self.value < other.value);
}

bool operator > (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return (self.value > other.value);
}

bool operator <= (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return (self < other) || (self == other);
}

bool operator >= (ExtOverloadOpStuct const& self, OverloadOpStuct2 const& other)
{
	return (self.value >= other.value);
}

//************************ logical operators **************************************************
bool operator ! (ExtOverloadOpStuct const& self)
{
	return self.value == 0;
}

bool operator && (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (!!self) && (!!other);
}

bool operator || (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return (!!self) || (!!other);
}

//************************ Bitwise operators **************************************************
ExtOverloadOpStuct operator ~ (ExtOverloadOpStuct const& self)
{
	return ExtOverloadOpStuct(~(self.value));
}

ExtOverloadOpStuct operator & (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return ExtOverloadOpStuct(self.value & other.value);
}

ExtOverloadOpStuct operator | (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return ExtOverloadOpStuct(self.value | other.value);
}

ExtOverloadOpStuct operator ^ (ExtOverloadOpStuct const& self, ExtOverloadOpStuct const& other)
{
	return ExtOverloadOpStuct(self.value ^ other.value);
}

ExtOverloadOpStuct operator << (ExtOverloadOpStuct const& self, int count)
{
	return ExtOverloadOpStuct(self.value << count);
}

ExtOverloadOpStuct operator >> (ExtOverloadOpStuct const& self, int count)
{
	return ExtOverloadOpStuct(self.value >> count);
}

// Member and pointer operators

int const* operator & (ExtOverloadOpStuct const& self)
{
	return &self.value;
}

int* operator & (ExtOverloadOpStuct& self)
{
	return &self.value;
}

void check_extern_overloaded_operator()
{
	//****************   Unary operators  *********************************************************
	{
		ExtOverloadOpStuct i(18);
		CHECK((-i).value == -18);
		CHECK((+i).value == +18);
		CHECK((~i).value == ~18);
		CHECK((*i).value == 18 * 18);
		// Pre incr and decr
		CHECK((++i).value == 19);
		CHECK(i.value == 19);
		CHECK((--i).value == 18);
		CHECK(i.value == 18);
		// post incr and decr
		CHECK((i++).value == 18);
		CHECK(i.value == 19);
		CHECK((i--).value == 19);
		CHECK(i.value == 18);
	}

	//********************* Binary operators ******************************************************
	// Arithmetic operators
	{
		ExtOverloadOpStuct a(6);
		ExtOverloadOpStuct b(3);

		ExtOverloadOpStuct mu = a * b;
		CHECK(mu.value == 18);

		ExtOverloadOpStuct d = a / b;
		CHECK(d.value == 2);

		ExtOverloadOpStuct p = a + b;
		CHECK(p.value == 9);

		ExtOverloadOpStuct mi = a - b;
		CHECK(mi.value == 3);

		mu /= b;
		CHECK(mu.value == a.value);

		d *= b;
		CHECK(d.value == a.value);

		p -= b;
		CHECK(p.value == a.value);

		mi += b;
		CHECK(mi.value == a.value);
	}

	// relational operators
	{
		ExtOverloadOpStuct a6(6);
		ExtOverloadOpStuct b6(6);
		ExtOverloadOpStuct c3(3);
		CHECK(b6 == a6);
		CHECK(b6 <= a6);
		CHECK(b6 >= a6);
		CHECK(c3 != a6);
		CHECK(c3  < a6);
		CHECK(c3 <= a6);
		CHECK(a6  > c3);
		CHECK(a6 >= c3);
	}

	// Asymetric relational operators
	{
		OverloadOpStuct2 a6 = { 6 };
		ExtOverloadOpStuct b6(6);
		ExtOverloadOpStuct c3(3);
		ExtOverloadOpStuct d9(9);
		CHECK(b6 == a6);
		CHECK(b6 <= a6);
		CHECK(b6 >= a6);
		CHECK(c3 != a6);
		CHECK(c3  < a6);
		CHECK(c3 <= a6);
		CHECK(d9  > a6);
		CHECK(d9 >= a6);
	}

	// Logical operators
	{
		ExtOverloadOpStuct a0(0);
		ExtOverloadOpStuct b0(0);
		ExtOverloadOpStuct c1(48);
		ExtOverloadOpStuct d1(79);
		CHECK(!a0);
		CHECK(!!c1);
		CHECK(!(a0 && b0));
		CHECK(!(a0 && c1));
		CHECK(!(c1 && a0));
		CHECK(c1 && d1);
		CHECK(!(a0 || b0));
		CHECK(a0 || c1);
		CHECK(c1 || a0);
		CHECK(c1 || d1);
	}

	//************************ Bitwise operators **************************************************
	{
		auto inv = ~decltype(ExtOverloadOpStuct::value)(0);
		ExtOverloadOpStuct a000(0);
		ExtOverloadOpStuct a110(6);
		ExtOverloadOpStuct a011(3);
		CHECK((~a000).value == inv);
		CHECK((a110 & a011) == ExtOverloadOpStuct(2));
		CHECK((a110 | a011) == ExtOverloadOpStuct(7));
		CHECK((a110 ^ a011) == ExtOverloadOpStuct(5));
		CHECK((a110 >> 1) == ExtOverloadOpStuct(3));
		CHECK((a110 << 1) == ExtOverloadOpStuct(12));
	}
}

void check_lib_porting_pair()
{
	std::pair<int, double> toto(3, 6.66);
	CHECK(toto.first == 3);
	CHECK(toto.second == 6.66);
}


void check_union()
{
	union U
	{
		int i;
		double d;
		bool b;
	} u;
	u.i = 45;
	CHECK(u.i == 45);
	u.d = 6.66;
	CHECK(u.d == 6.66);

	struct Foo
	{
		union
		{
			int i;
			double d;
			bool b;
		};
		Foo(bool b2) : b(b2) {}

	} foo(true);
	CHECK(foo.b == true);
	foo.i = 45;
	CHECK(foo.i == 45);
	foo.d = 6.66;
	CHECK(foo.d == 6.66);
}

template<typename F, typename A>
auto call_func(F f, A v) //->decltype(F())
{
	return f(v);
}

void check_lambda()
{
	auto give_1 = []() {return 1; };
	CHECK(give_1() == 1);

	auto give_x = [](int x) {return x; };
	CHECK(give_x(1) == 1);
	CHECK(call_func(give_x, 2) == 2);
	CHECK(call_func([](int x) {return x; }, 3) == 3);

	auto give_tx = [](auto x) {return x; };
	CHECK(give_tx(1) == 1);
	CHECK(call_func(give_tx, -12) == -12);
	CHECK(call_func([](auto x) {return x; }, 6.66) == 6.66);

	auto to_1 = [](int& x) {x = 1; };
	int x;
	to_1(x);
	CHECK(x == 1);

	auto ext_to_2 = [&x]() {x = 2; };
	ext_to_2();
	CHECK(x == 2);

	auto give_x_or_y = [](bool s, int x, int y) -> int {if (s) return x; else return y; };
	CHECK(give_x(1) == 1);

	auto give_tx_plus_ty = [](auto x, auto y) {return x + y; };
	CHECK(give_tx_plus_ty(1, 3.33) == 4.33);

	auto copy_a_to_b = [](auto a, auto& b) {b = a; };
	int b = 0;
	copy_a_to_b(958421, b);
	CHECK_EQUAL(b, 958421);

	auto copy_42_to_b = [](auto & b) {b = 42; return 18; };
	CHECK_EQUAL(copy_42_to_b(b), 18);
	CHECK_EQUAL(b, 42);

	auto set_string_to_toto = [](std::string& b) {b = "toto"; };
	std::string toto;
	set_string_to_toto(toto);
	CHECK_EQUAL(toto, "toto");
}

struct StructWithDefCtor
{
	int i = 42;
};

struct StructWithoutDefCtor
{
	int i = 42;

	StructWithoutDefCtor(int = 0)
	{
		i = 2;
	}
};

void check_struct_default_ctor()
{
	StructWithDefCtor foo;
	CHECK(foo.i == 42);
	CHECK(StructWithDefCtor().i == 42);

	StructWithoutDefCtor foo2;
	CHECK(foo2.i == 2);
	CHECK(StructWithoutDefCtor().i == 2);
}

struct Struct781
{
	int i = 0;
	Struct781() = default;
	Struct781(int u) :i(u) {};
};

struct Struct782
{
	Struct781 toto;

	Struct782() = default;
	Struct782(int i) : toto(i) {};
	Struct782(double) {};
};

int take_struct(Struct781 const& s)
{
	return s.i;
}

void check_struct_ctor_call()
{
	Struct782 tutu;
	CHECK(tutu.toto.i == 0);
	Struct782 tutu2(5);
	CHECK(tutu2.toto.i == 5);
	Struct782 tutu22(5.5);
	CHECK(tutu22.toto.i == 0);
	Struct782 tutu3 = 6;
	CHECK(tutu3.toto.i == 6);
	CHECK(take_struct(Struct781(57)) == 57);
}

class Class781
{
public:
	int i = 0;
	Class781() {};
	Class781(int u):i(u) {};
};

class Class782
{
public:
	Class781 toto;
	Class781* toto2 = nullptr;
	Class781* toto3;

	//Class782():toto2(new Class781()) {};
	Class782() = default;
	Class782(int i) : toto(i), toto2(new Class781(i)) {};
	Class782(double) : toto2(new Class781(19)) {};

	~Class782() { delete toto2; }
};

Class781 const& take_class_(Class781 const& s)
{
	return s;
}

Class781* take_class(Class781* s)
{
	return s;
}

class CheckNoCtor
{

};

class InheritCheckNoCtor : public CheckNoCtor
{
public:
	InheritCheckNoCtor() :CheckNoCtor()
	{
	}
};

class CheckWithDefCtor
{
public:
	CheckWithDefCtor() {}
};

class InheritWithDefCtor : public CheckWithDefCtor
{
public:
	InheritWithDefCtor() :CheckWithDefCtor()
	{
	}
};

class CheckWithDefDefCtor
{
public:
	CheckWithDefDefCtor() {}
};

class InheritWithDefDefCtor : public CheckWithDefDefCtor
{
public:
	InheritWithDefDefCtor() :CheckWithDefDefCtor()
	{
	}
};

class CheckWithCtor
{
public:
	int i;
	CheckWithCtor(int u) 
		:i(u)
	{
	}
};

class InheritCheckWithCtor : public CheckWithCtor
{
public:
	InheritCheckWithCtor() :CheckWithCtor(72)
	{
	}
};

void check_class_ctor_call()
{
	Class782 tutu;
	CHECK(tutu.toto.i == 0);
	Class782 tutu2(5);
	CHECK(tutu2.toto.i == 5);
	Class782 tutu22(5.5);
	CHECK(tutu22.toto2->i == 19);
	Class782 tutu3 = 6;
	CHECK(tutu3.toto.i == 6);
	CHECK(take_class_(Class781(57)).i == 57);

	Class782* tutu4 = new Class782();
	CHECK(tutu4->toto.i == 0);
	delete tutu4;
	Class782* tutu5 = new Class782(5);
	CHECK(tutu5->toto.i == 5);
	delete tutu5;
	CHECK(take_class_(Class781(57)).i == 57);

	Class781* tutu6 = take_class(new Class781(58));
	CHECK(tutu6->i == 58);
	tutu6->~Class781();
	//::operator delete(tutu6);

	InheritCheckNoCtor* tutu7 = new InheritCheckNoCtor;
	CheckNoCtor* tutu8 = tutu7;
	CHECK(tutu8 != nullptr);

	InheritCheckWithCtor* tutu9 = new InheritCheckWithCtor;
	CheckWithCtor* tutu10 = tutu9;
	CHECK(tutu10 != nullptr);
	CHECK(tutu10->i == 72);

	InheritWithDefCtor* tutu11 = new InheritWithDefCtor;
	CheckWithDefCtor* tutu12 = tutu11;
	CHECK(tutu12 != nullptr);

	InheritWithDefDefCtor* tutu13 = new InheritWithDefDefCtor;
	CheckWithDefDefCtor* tutu14 = tutu13;
	CHECK(tutu14 != nullptr);
}

void func_throw_except()
{
	throw std::runtime_error("error test");
}
/*
void func_throw_except(std::runtime_error const& ex)
{

}
*/
void check_exception()
{
	bool catched = false;
	try
	{
		//func_throw_except(std::runtime_error("toto"));
		func_throw_except();
	}
	catch (std::logic_error&)
	{

	}
	catch (std::runtime_error& ex)
	{
		CHECK(strcmp(ex.what(), "error test") == 0);
		catched = true;
	}
	catch (std::exception&)
	{

	}
	CHECK(catched);
}

void check_exception2()
try
{
	//func_throw_except(std::runtime_error("toto"));
	func_throw_except();
	CHECK(false);
}
catch (std::logic_error&)
{
	CHECK(false);
}
catch (std::runtime_error& ex)
{
	CHECK(strcmp(ex.what(), "error test") == 0);
}
catch (std::exception&)
{
	CHECK(false);

}

void check_for_loop()
{
	for (int x = 0, y = 3; x != y; ++x);
	size_t count = 0;
	for (int i = 0; i < 3; ++i)
		++count;
	CHECK(count == 3);
	count = 0;
	int i;
	for (i = 0; i < 3; ++i)
		++count;
	CHECK(count == 3);
	count = 0;
	for (int x = 0, y = 3; x != y; ++x)
		++count;
	CHECK(count == 3);
}

void check_while_loop()
{
	int count = 3;
	while (count != 0) --count;
	CHECK(count == 0);
	count = 3;
	while (--count);
	CHECK(count == 0);
}

void check_dowhile_loop()
{
	int count = 3;
	do
	{
		--count;
	}
	while (count != 0);
	CHECK(count == 0);
	count = 3;
	do
	{

	}
	while (--count);
	CHECK(count == 0);
}


void check_multidecl_line()
{
	int i = 2, *u = new int[18], z[12];
	CHECK(i == 2);
	CHECK(sizeof(z) / sizeof(int) == 12);
	z[3] = 77;
	CHECK(z[3] == 77);
	u[17] = 78;
	CHECK(u[17] == 78);
}

struct ArrayStruct
{
	std::array<int, 3> tab = { { 0, 0, 0 } };

	ArrayStruct(int a, int b, int c)
	{
		tab[0] = a; tab[1] = b; tab[2] = c;
	}
};


struct ArrayArrayStruct
{
	ArrayStruct tab2 = ArrayStruct(0, 0, 0);
};

void check_std_array()
{
	ArrayStruct tab2(0, 0, 0);
	ArrayArrayStruct toto;
	CHECK(toto.tab2.tab[0] == 0);

	std::array<int, 3> tab3 = { { 4, 5, 6 } };
	tab3.fill(18);
	CHECK(tab3[0] == 18);
}


struct ImplCtorStruct
{
	int i;

	ImplCtorStruct(int u) : i(u) {}
};

class ImplCtorClass
{
public:
	int i;
	ImplCtorClass(int u) : i(u) {}
};

void check_implicit_ctor()
{
	ImplCtorStruct a = 2;
	CHECK(a.i == 2);
	ImplCtorStruct t[3] = {1, 2, 3};
	CHECK(t[2].i == 3);
	ImplCtorClass b = 18;
	CHECK(b.i == 18);
	ImplCtorClass y[3] = { 1, 2, 3 };
	CHECK(y[2].i == 3);
}

class WithExternBody
{
public:
	
	int give42(int i = 1);

	int give43()
	{
		return 43;
	}
};

int WithExternBody::give42(int tu)
{
	return 42 * tu;
}

int give42(int i = 1);

int give42(int tu)
{
	return 42 * tu;
}


int give43()
{
	return 43;
}

void check_extern_methode()
{
	WithExternBody toto;
	CHECK(toto.give42() == 42);
	CHECK(give42() == 42);
	CHECK(toto.give43() == 43);
	CHECK(give43() == 43);
}

class SimpleClass
{
public:
	int i;

	SimpleClass() = default;
	SimpleClass(SimpleClass const&) = default;
	SimpleClass* dup()
	{
		return new SimpleClass(*this);
	}
};

struct ContainScopedClass
{
	SimpleClass m1;
};

struct ContainScopedClass2
{
	SimpleClass m1;

	ContainScopedClass2() = default;

	ContainScopedClass2(ContainScopedClass2 const& other)
		: m1(other.m1)
	{
	}
};

void check_struct_containing_scooped_class()
{
	{
		ContainScopedClass c1;
		c1.m1.i = 98;
		auto c2 = c1;
		c1.m1.i = 6;
		CHECK(c2.m1.i == 98);
		c2 = c1;
		c1.m1.i = 7;
		CHECK(c2.m1.i == 6);
	}

	{
		ContainScopedClass2 c1;
		c1.m1.i = 98;
		auto c2 = c1;
		c1.m1.i = 6;
		CHECK(c2.m1.i == 98);
		c2 = c1;
		c1.m1.i = 7;
		CHECK(c2.m1.i == 6);
	}

}

class CopyDisableClass
{
public:
	int i = 3;
	CopyDisableClass(){}

	CopyDisableClass(CopyDisableClass const&) = delete;
	CopyDisableClass& operator=(CopyDisableClass const&) = delete;
};

class CopyDisableClass2
{
	CopyDisableClass2(CopyDisableClass2 const&);
	CopyDisableClass2& operator=(CopyDisableClass2 const&);

public:
	int i = 3;
	CopyDisableClass2() {}
};

void check_not_copyable_class()
{
	CopyDisableClass a;
	CHECK(a.i == 3);
	CopyDisableClass2 b;
	CHECK(b.i == 3);
}

void check_break()
{
	int i = 0;
	for (; i < 10; ++i)
	{
		if (i == 5)
			break;
	}
	CHECK(i == 5);
}

void check_continue()
{
	int i = 0;
	for (; i < 10; ++i)
	{
		if (i == 5)
			continue;
		CHECK(i != 5);
	}
	CHECK(i == 10);
}

void check_switch()
{
	auto test = [](int value)
	{
		int res;
		switch (value)
		{
		case 0: return 0;
		case 1: return 1;
		case 2: res = 2; break;
		default: res = 3;
		}
		return res;
	};
	CHECK(test(0) == 0);
	CHECK(test(1) == 1);
	CHECK(test(2) == 2);
	CHECK(test(3) == 3);

	enum EA 
	{
		OPT2,
		OPT21 = OPT2,
		OPT3
	};

	auto test_named_enum = [](EA a)
	{
		int result = 0;
		switch (a)
		{
		case OPT2: result = 1; break;
		case OPT3: result = 2;  break;
		}
		return result;
	};
	CHECK(test_named_enum(OPT2) == 1);
	CHECK(test_named_enum(OPT21) == 1);
	CHECK(test_named_enum(OPT3) == 2);
	CHECK(test_named_enum((EA)-1) == 0);

	enum class EB
	{
		OPT4,
		OPT41 = OPT4,
		OPT5
	};

	auto test_class_enum = [](EB a)
	{
		int result = 0;
		switch (a)
		{
		case EB::OPT4: result = 1;  break;
		case EB::OPT5: result = 2;  break;
		}
		return result;
	};
	CHECK(test_class_enum(EB::OPT4) == 1);
	CHECK(test_class_enum(EB::OPT41) == 1);
	CHECK(test_class_enum(EB::OPT5) == 2);
	CHECK(test_class_enum((EB)-1) == 0);

	auto test_enum_fallthrough = [](EB a) 
	{
		int result = 0;
		switch (a)
		{
		case EB::OPT4:
			result = 2;
		case EB::OPT5:
			++result;
			break;
		}
		return result;
	};
	CHECK(test_enum_fallthrough(EB::OPT4) == 3);
	CHECK(test_enum_fallthrough(EB::OPT41) == 3);
	CHECK(test_enum_fallthrough(EB::OPT5) == 1);
	CHECK(test_enum_fallthrough((EB)-1) == 0);
}

extern int ext_var;

int ext_var = 18;

void check_ext_vat()
{
	CHECK(ext_var == 18);
}

template<typename T>
struct TableTraits2;

template<typename T, size_t I>
struct TableTraits2<T[I]>
{
	static size_t const Size = I;
};

template<typename T>
struct TableTraits;

template<>
struct TableTraits<int[6]>
{
	static size_t const Size = 6;
};


void check_tmpl_sized_array()
{
	int tab[] = { 0, 1, 2, 3, 4, 5 };
	static_assert(TableTraits<decltype(tab)>::Size == 6, "");
	static_assert(TableTraits2<decltype(tab)>::Size == 6, "");
}

void check_rethrow()
{
	int status = 0;
	try
	{
		try
		{
			try
			{
				throw std::runtime_error("test");
			}
			catch (std::runtime_error& ex)
			{
				CHECK(status == 0);
				++status;
				throw;
			}
		}
		catch (std::runtime_error&)
		{
			CHECK(status == 1);
			++status;
			throw;
		}
	}
	catch (std::runtime_error&)
	{
		CHECK(status == 2);
		++status;
	}
	CHECK(status == 3);

	try
	{
		try
		{
			throw std::runtime_error("test");
		}
		catch (...)
		{
			++status;
			throw;
		}
	}
	catch (...)
	{
		CHECK(status == 4);
		++status;
	}
	CHECK(status == 5);
}

typedef struct Z Z;

struct Z
{
	enum Y : char
	{
		A,
		B,
		C
	};
};

void check_enum()
{
	CHECK_EQUAL(Z::Y::A, 0);
	CHECK_EQUAL(Z::Y::B, 1);
	CHECK_EQUAL(Z::Y::C, 2);

	CHECK_EQUAL(Z::A, 0);
	CHECK_EQUAL(Z::B, 1);
	CHECK_EQUAL(Z::C, 2);

	Z::Y y = Z::B;
	CHECK_EQUAL(y, Z::B);
}

void check_cast()
{
	double d = 3.33;
	int i = int(d);
	CHECK_EQUAL(i, 3);
}

void check_keyword_convertion()
{
	int version = 0;
	CHECK_EQUAL(version, 0);
	int out = 0;
	CHECK_EQUAL(out, 0);
	int in = 0;
	CHECK_EQUAL(in, 0);
	int ref = 0;
	CHECK_EQUAL(ref, 0);
	int debug = 0;
	CHECK_EQUAL(debug, 0);
	int function = 0;
	CHECK_EQUAL(function, 0);
	int cast = 0;
	CHECK_EQUAL(cast, 0);
	int align = 0;
	CHECK_EQUAL(align, 0);
	int Exception = 0;
	CHECK_EQUAL(Exception, 0);
}

enum 
{
	a,
	b,
	c = 32,
	d,
	e = 1,
	f
};

void check_anonymous_enum()
{
	static_assert(a == 0, "");
	static_assert(b == 1, "");
	static_assert(c == 32, "");
	static_assert(d == 33, "");
	static_assert(e == 1, "");
	static_assert(f == 2, "");
}

void func_with_pointer(Struct781* s, SimpleClass* s2)
{
	int u = s->i;
	CHECK_EQUAL(u, 5678);
	int v = s2->i;
	CHECK_EQUAL(v, 1234);
}

void check_not_array_ptr()
{
	SimpleClass* b = new SimpleClass;
	b->i = 1234;

	Struct781 s;
	s.i = 5678;
	func_with_pointer(&s, b);
	delete b;
}


void check_string_literal()
{
	char a[] = "\x53\x80\xF6\x34";	// Not an utf-8 string
	char b[] = "漢字";				// utf-8 (Need Unicode to be printed)
}

void test_register(TestFrameWork& tf)
{
	auto ts = std::make_unique<TestSuite>();

	ts->addTestCase(check_string_literal);

	ts->addTestCase(check_cast);

	ts->addTestCase(check_enum);

	ts->addTestCase(check_operators);

	ts->addTestCase(check_function);

	ts->addTestCase(check_static_array);

	ts->addTestCase(check_dynamic_array);

	ts->addTestCase(check_struct);

	ts->addTestCase(check_template_struct_specialization);

	ts->addTestCase(check_template_function);

	ts->addTestCase(check_class);

	ts->addTestCase(check_type_alias);

	ts->addTestCase(check_bitfield);

	ts->addTestCase(check_decayed_type);

	ts->addTestCase(check_injectedclassnametype);

	ts->addTestCase(check_convertion_operator);

	ts->addTestCase(check_abstract_keyword);

	ts->addTestCase(check_class_opassign);

	ts->addTestCase(check_tmpl_func_spec);

	ts->addTestCase(check_variadic);

	ts->addTestCase(check_tmpl_meth);

	ts->addTestCase(check_function_pointer);

	ts->addTestCase(check_method_pointer);

	ts->addTestCase(check_member_pointer);

	ts->addTestCase(check_uninstantiated_default_arg);

	ts->addTestCase(check_class_instantiation);

	ts->addTestCase(check_incomplete_array_type);

	ts->addTestCase(check_builtin_macro);

	ts->addTestCase(check_incr_pointer);

	ts->addTestCase(check_function_macro);

	ts->addTestCase(check_range_based_for_loop);

	ts->addTestCase(check_overloaded_operator);

	ts->addTestCase(check_extern_overloaded_operator);

	ts->addTestCase(check_lib_porting_pair);

	ts->addTestCase(check_union);

	ts->addTestCase(check_lambda);

	ts->addTestCase(check_struct_default_ctor);

	ts->addTestCase(check_struct_ctor_call);

	ts->addTestCase(check_class_ctor_call);

	ts->addTestCase(check_exception);

	ts->addTestCase(check_exception2);

	ts->addTestCase(check_for_loop);

	ts->addTestCase(check_while_loop);

	ts->addTestCase(check_dowhile_loop);

	ts->addTestCase(check_multidecl_line);

	ts->addTestCase(check_std_array);

	ts->addTestCase(check_implicit_ctor);

	ts->addTestCase(check_extern_methode);

	ts->addTestCase(check_struct_containing_scooped_class);

	ts->addTestCase(check_not_copyable_class);

	ts->addTestCase(check_break);

	ts->addTestCase(check_continue);

	ts->addTestCase(check_switch);

	ts->addTestCase(check_ext_vat);

	ts->addTestCase(check_tmpl_sized_array);

	ts->addTestCase(check_rethrow);

	ts->addTestCase(check_keyword_convertion);
	
	ts->addTestCase(check_anonymous_enum);

	ts->addTestCase(check_not_array_ptr);

	tf.addTestSuite(std::move(ts));
}