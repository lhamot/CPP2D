#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ciso646>

class Tata
{
public:
	virtual int funcV()
	{
		return 1;
	}

	int funcF()
	{
		return 1;
	}
};

class Tata2 : public Tata
{
public:
	int funcV() override
	{
		return 2;
	}

	int funcF(int i = 0)
	{
		return 2;
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
class TmplClass2 : public TmplClass1<I>{};

template<>
class TmplClass2<0> : public TmplClass1<42>{};

unsigned int testCount = 0;

void check(bool ok, char const* message, int line)
{
	++testCount;
	if (not ok)
	{
		//printf(message);
		printf("%s    ->Failed at line %u\n", message, line);
	}
}

#define CHECK(COND) check(COND, #COND, __LINE__) //if(!(COND)) error(#COND)

template<typename T1, typename T2>
bool fl_equal(T1 v1, T2 v2)
{
	auto diff = v1 - v2;
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

void check_array()
{
	int tab[3] = { 1, 2, 3 };
	tab[1] = 42;
	CHECK(tab[1] == 42);
	CHECK(tab[2] == 3);

	int tab3[] = { 4, 5, 6, 7 };
	CHECK(sizeof(tab3) == 4 * sizeof(int));

	int *tab2 = new int[46];
	tab2[6] = 42;
	CHECK(tab2[6] == 42);
	delete[] tab2;
	tab2 = nullptr;

	int tabA[3] = { 1, 2, 3 };
	int tabB[3] = { 1, 2, 3 };
	CHECK(tabA != tabB);

	int *tabC = new int[3];
	int *tabD = new int[3];
	for (int i = 0; i < 3; ++i)
	{
		tabC[i] = i - 1;
		tabD[i] = i - 1;
	}
	CHECK(tabD != tabC);
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

	//virtual void impossible(){}
};

short Toto3::u = 36;


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

template<typename T3>
struct is_same<T3, T3>
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
	Tata* t = t2;
	CHECK(t2->funcV() == 2);
	CHECK(t2->funcF() == 1);
	CHECK(t2->getThis() == t2);
	CHECK(&t2->getThisRef() == t2);
	delete t;

	CHECK(TmplClass2<3>::value == 3);
	CHECK(TmplClass2<0>::value == 42);
}

int main()
{
	check_operators();

	check_function();

	check_struct();

	check_array();

	check_template_struct_specialization();

	check_template_function();

	printf("%u tests\n", testCount);

	return EXIT_SUCCESS;
}