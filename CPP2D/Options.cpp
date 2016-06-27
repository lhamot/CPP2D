#include "Options.h"

Options& Options::getInstance()
{
	static Options instance;
	return instance;
}
