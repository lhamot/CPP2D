//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Check for #ifdef #endif

#define FEATURE_A

#ifdef FEATURE_A
int const FeatureA = 1;
#else
int const FeatureA = 0;
#endif

static_assert(FeatureA == 1, "FEATURE_A is not defined??");

// Check for #if

#define FEATURE_B

#if defined(FEATURE_B)
int const FeatureB = 1;
#else
int const FeatureB = 0;
#endif

static_assert(FeatureB, "FEATURE_B is not defined??");

// Check for #else

#if defined(FEATURE_C)
int const FeatureC = 1;
#else
int const FeatureC = 0;
#endif

static_assert(FeatureC == 0, "FEATURE_C is defined??");

// Check for #elif

#if defined(FEATURE_D)
	int const FeatureD = 1;
#elif defined(FEATURE_B)
	int const FeatureD = 2;
#else
	int const FeatureD = 3;
#endif

static_assert(FeatureD == 2, "FEATURE_D is defined??");

void func4845(
	int aaaa
	, int bbbb //hhhh!
	, int cccc = 45 //iii!
	, char const* dddd = "ffffgggg" //jjjj!
	, char const* uuuu = "ojftfs", //xkdtrt!
	int eeee = 13
)
{
}

int func2(int asdflkhds, int bateau)
{
	/*
	  gdf
	  dslksdgfjn
	  gdfg
	*/
	int a = asdflkhds;
	/*
	sdgfdfhfgj
	*/
	return a;
	/*
	- 1:
	  - 2
	  - 3
	*/
}

// Comment!
int func3()
{
	//ABCDEF1
	int a = 0;
	//ABCDEF2
	return a;
	//ABCDEF3
}

// Comment!
int func4()
{
#ifdef ABCDEF4
	assert(false);
#endif
	int a = 0;
#ifdef ABCDEF5
	assert(false);
#endif	
	return a;
#ifdef ABCDEF6
	assert(false);
#endif	
}

/*
Flub
*/