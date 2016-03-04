#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ciso646>

void check(bool ok, char const* message, int line)
{
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
	int tab[3] = {1, 2, 3};
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

	Toto toto2 = { 7, 8.99, true };
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
	};

	Toto2 toto3(18, 12.57, true);
	CHECK(toto3.a == 18 && fl_equal(toto3.b, 12.57) && toto3.c == true);

	struct Toto3
	{
		int a;
		float b;
		bool c;

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

	Toto3 t3;
	t3.setA(16);
	CHECK(t3.getA() == 16);
	CHECK(Toto3::getStatic() == 12);
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

int main()
{
	check_operators();
	
	check_function();

	check_struct();

	check_array();

	check_template_struct_specialization();

	check_template_function();

	return EXIT_SUCCESS;
}