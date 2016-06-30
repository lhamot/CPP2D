import std.container.rbtree;
import std.container.array;
import std.typecons;
import std.stdio;

//******************************  map *****************************************
struct pair(K, V)
{
	alias first_type = K;
	alias second_type = V;

	first_type first;
	second_type second;

	bool opEquals(in pair s) const
	{
		return first == s.first;
	}

	int opCmp(in pair s) const
	{
		return 
			first < s.first? -1:
			first > s.first? 1:
			0;
	}

	ref first_type key() {return first;}
	ref second_type value() {return second;}
}

alias map(K, V, alias less = "a < b") = RedBlackTree!(pair!(K, V), less, false);
alias multimap(K, V, alias less = "a < b") = RedBlackTree!(pair!(K, V), less, true);


//**************************  boost::serialization ****************************

template class_version(T)
{
	immutable(int) class_version = 0;
}

template class_version(T: double)
{
	immutable(int) class_version = 6;
}

//************************** std::vector **************************************

struct allocator(T){}

alias vector(T, A = allocator!T) = Array!T;

//************************** std::set **************************************

struct set(T, alias less = "a < b", A = allocator!T)
{
	RedBlackTree!(T, less) data = new RedBlackTree!(T, less);

	auto length() {return data.length;}

	this()(auto this)
	{
		data = data.dup;
	}
}


//************************** unordered_map **************************************

struct unordered_map(K, V)
{
	V[K] ptr;
}

//************************* cpp-like lambda helper ****************************

struct toFunctor(alias F)
{
	this(int){}
	auto ref opCall(Args...)(auto ref Args args)
	{
		return F(args);
	}
}  

//************************* default ctor helper ****************************

struct DefaultCtorArgType{};
static DefaultCtorArgType DefaultCtorArg;

// ********************** std::move ****************************************

auto move(T)(auto ref T ptr) if(is(T == struct))
{
	auto newPtr = ptr;
	ptr = T.init;
	return newPtr;
}

auto move(T)(auto ref T ptr) if(is(T == class))
{
	auto newPtr = ptr;
	ptr = null;
	return newPtr;
}

// ******************************** <utility> *********************************

auto make_pair(A, B)(auto ref A a, auto ref B b)
{
	//return Tuple!(A, "key", B, "value")(a, b);
	return pair!(A, B)(a, b);
}

// ******************************* other **************************************

struct Ref(T)
{
	T* ptr;

	this(U)(auto ref U other)
	{
		ptr = &other;
	}

	ref T opCast()
	{
		return *ptr;
	}

	bool opBinary(string op, U)(auto ref U other)
	{
		return mixin("*ptr" ~ op ~ "other");
	}

	bool opBinary(string op, U = Ref!U)(auto ref Ref!U other)
	{
		return mixin("*ptr" ~ op ~ "*other.ptr");
	}

	alias ptr this;
}

auto makeRef(T)(ref auto T val)
{
	return Ref!T(val);
}

//************************* iostream ******************************************

struct OStream
{
	File file;

	ref OStream opBinary(string op = "<<", T)(auto ref T arg)
	{
		file.write(arg);
		return this;
	}
}

//
/*import mysql.protocol.commands;

auto ref Command bindInputs(Args...)(auto ref Command command, Args args)
{
	command.bindParameterTuple(args);
	return command;
}*/

