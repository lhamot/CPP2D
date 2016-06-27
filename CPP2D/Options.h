#pragma once

#include <string>
#include <unordered_map>

struct TypeOptions
{
	//! Semantic of an object (Value or Reference)
	enum Semantic
	{
		Value,      //!< Like **D** struct and everything in <b>C++</b>
		Reference,  //!< Garbage collected like **D** class
		AssocArray, //!< Special case for hashmap. Created without new, but reference semantic.
	};

	std::string name;
	Semantic semantic;
};

struct Options
{
	std::unordered_map<std::string, TypeOptions> types;

	static Options& getInstance();
};

