//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

// Comment! 1
void func(int asdflkhds = 36, int bateau = 42)
{
#define ABCDEF
}

int func2()
{
#define ABCDEF1
	int a = 0;
#define ABCDEF2
	return a;
#define ABCDEF3
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