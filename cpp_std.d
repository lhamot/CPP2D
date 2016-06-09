import std.container.rbtree;
import std.container.array;
import std.typecons;

//******************************  map *****************************************
struct pair(K, V)
{
	alias first_type = K;
	alias second_type = V;

	first_type first;
	second_type second;

	bool opEquals()(auto pair s) const pure @safe
	{
		return first == s.first;
	}

	int opCmp()(auto pair s) const pure @safe
	{
		return 
			first < s.first? -1:
		first > s.first? 1:
		0;
	}
}

alias map(K, V, alias less = "a < b") = RedBlackTree!(pair!(K, V), less, true);


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

//************************** unordered_map **************************************

struct unordered_map(K, V)
{
	V[K] ptr;
}

//************************* cpp-like lambda helper ****************************

struct toFunctor(alias F)
{
	this(int){}
	auto opCall(Args...)(Args args)
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
	return Tuple!(A, "key", B, "value")(a, b);
}