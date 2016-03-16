#include <cstdio>
#include "framework.h"

unsigned int testCount = 0;

void check(bool ok, char const* message, int line)
{
	++testCount;
	if (!ok)
	{
		//printf(message);
		printf("%s    ->Failed at line %u\n", message, line);
	}
}

void print_results()
{
	printf("%u tests\n", testCount);
}