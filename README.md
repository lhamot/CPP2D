# CPP2D
Clang based C++ to D language converter

## Objective
The goal is to take a C++ project and convert all the source code to D language.

## Some example of converted code

### class

#### Base
```C++
// C++
class Widget {
	Widget(Widget const&);
	Widget& operator=(Widget const&);
	Widget* parent_;
	virtual void displayImpl() = 0;
	virtual void onClickImpl(size_t x, size_t y) = 0;
public:
	Widget(Widget* parent = nullptr): parent_(parent) {}
	~Widget() = default;
	void display() { displayImpl(); }
	void onClick(size_t x, size_t y) { onClickImpl(x, y); }
};
```
```D
// D
class Widget
{
    Widget parent_;
    abstract void displayImpl();
    abstract void onClickImpl(
        size_t x,
        size_t y);
public:
    this(Widget parent = null)
    {
        parent_ = parent;
    }
    final void display()
    {
        displayImpl();
    }
    final void onClick(
        size_t x,
        size_t y)
    {
        onClickImpl(x, y);
    }
}
```
- **virtual** keyword is removed
- **= 0** become **abstract**
- No-virtual methods become **final**
- Constructor are renamed **this**

#### Inheritance
```C++
// C++
class ChildWidget : public Widget {
	void displayImpl() override { printf("displayImpl\n"); }
	void onClickImpl(size_t x, size_t y) { printf("onClickImpl %u %u\n", x, y); }
public:
	ChildWidget(Widget* parent = nullptr) :Widget(parent) {}
};
```
```D
// D
class ChildWidget : Widget
{
    override void displayImpl()
    {
        writef("displayImpl\n\0");
    }
    override void onClickImpl(size_t x, size_t y)
    {
        writef("onClickImpl %u %u\n\0", x, y);
    }
public:
    this(Widget parent = null)
    {
        super(parent);
    }
}
```
- Inheritance and access specifiers are removed
- **override** is added on all overriding methods
- Call **super** for construct the base type

#### Usage
```C++ 
// C++
int main(){
	Widget* w = new ChildWidget();
	w->display();
	w->onClick(1, 2);
	delete w;
}
```
```D
// D
int main()
{ 
    Widget w = new ChildWidget(null); 
    w.display(); 
    w.onClick(1, 2); 
    w = null; 
}
```
- Remove pointer to class
- assign **null** in place of **delete**

### operator overload
```C++
// C++
struct Rational // Stupidly simple rational struct
{
	int num = 1;
	int den = 1;

	Rational(int num_, int den_ = 1) : num(num_), den(den_) {}

	bool operator == (Rational const& rhs) {
		return num == rhs.num && den == rhs.den;
	}
	bool operator != (Rational const& rhs) {
		return !(*this == rhs);
	}
	bool operator < (Rational const& rhs) {
		return num * rhs.den < rhs.num * den;
	}
	bool operator <= (Rational const& rhs) {
		return *this < rhs || *this == rhs;
	}
	bool operator >= (Rational const& rhs) {
		return !(*this < rhs);
	}
	bool operator > (Rational const& rhs) {
		return !(*this <= rhs);
	}
};

Rational& operator += (Rational& lhs, Rational const& rhs){
	lhs.num = lhs.num * rhs.den + rhs.num * lhs.den;
	lhs.den *= rhs.den;
	return lhs;
}

Rational operator + (Rational const& lhs, Rational const& rhs){
	Rational tmp = lhs;
	tmp += rhs;
	return tmp;
}

int operator * (int lhs, Rational const& rhs){
	lhs *= rhs.num;
	return lhs / rhs.den;
}
```
```D
// D
struct Rational
{
    int num = 1;
    int den = 1;
    this(int num_, int den_ = 1)
    {
        num = num_;
        den = den_;
    }
    bool opEquals(Rational rhs)
    { 
        return num == rhs.num && den == rhs.den; 
    }
    bool _opLess(Rational rhs)
    { 
        return num * rhs.den < rhs.num * den; 
    }
    ref Rational opOpAssign(string op: "+")(Rational rhs)
    {
        alias lhs = this; 
        lhs.num = lhs.num * rhs.den + rhs.num * lhs.den; 
        lhs.den *= rhs.den; 
        return lhs;
    }
    Rational opBinary(string op: "+")(Rational rhs)
    {
        alias lhs = this; 
        Rational tmp = lhs; 
        tmp += rhs; 
        return tmp;
    }
    int opBinaryRight(string op: "*")(int lhs)
    {
        alias rhs = this; 
        lhs *= rhs.num; 
        return lhs / rhs.den;
    }
    int opCmp(ref in Rational other)
    {
        return _opLess(other) ? -1: ((this == other)? 0: 1);
    }
}
```
- Constructor initializer list is printed to simple assignment operation
- Operators are renamed to the **D** operator name style
- Useless operators are prefixed by an underscore
- **opCmp** is create and use the old **operator<**, renamed **_opLess**, and **opAssign**
- Free operators are moved into the struct
- Free operator with **Rational** at the right are moved into the struct with the **Right** suffix, like **opBinaryRight**

### template

#### template function
```C++
// C++
template<typename T> 
T max(T a, T b) {
    return (a >= b)?  a: b; 
}
```
```D
// D
T max(T)(T a, T b)
{ 
    return (a >= b)? a: b;
}
```

#### template function specialization
```C++
// C++
template<typename T> void WhoAmI(T A);
template<> void WhoAmI<int>(int){
	printf("I am int");
}
template<> void WhoAmI<double>(double) {
	printf("I am double");
}
```
```D
// D
void WhoAmI(T)(T A);
void WhoAmI(T : int)(int var0)
{ 
    writef("I am int\0");
}
void WhoAmI(T : double)(double var0)
{ 
    writef("I am double\0");
}
```

#### template class specialization
```C++
// C++
template<typename T> struct TmpClass;
template<> struct TmpClass<int>{
	void WhoAmI() {
		printf("I am a <int> template\n");
	}
};
template<> struct TmpClass<double> {
	void WhoAmI() {
		printf("I am a <double> template\n");
	}
};
```
```D
// D
struct TmpClass(T);
struct TmpClass(T : int)
{
    void WhoAmI()
    { 
        writef("I am a <int> template\n\0"); 
    }
};
struct TmpClass(T : double)
{
    void WhoAmI()
    { 
        writef("I am a <double> template\n\0"); 
    }
};

```

#### template class partial specialization
```C++
// C++
template<typename T, bool DoMax> class Max;
template<typename T> class Max<T, true> {
public:
	static T get(T a, T b) {
		return (a >= b) ? a : b; //get the max
	}
};
template<typename T> class Max<T, false> {
public:
	static T get(T a, T b) {
		return (a < b) ? a : b; //get the min
	}
};
```
```D
// D
class Max(T, bool DoMax);
class Max(T : T_, bool DoMax : 1, T_)
{
public:
    static final T_ get(T_ a, T_ b)
    { 
        return (a >= b)? a: b; //get the max
    }
};
class Max(T : T_, bool DoMax : 0, T_)
{
public:
    static final T_ get(T_ a, T_ b)
    { 
        return (a < b)? a: b; //get the min
    }
};
```

### arrays

#### static
```C++
// C++
int static_array[] = { 0, 1, 2, 3 };
printf("%u\n", static_array[3]);
for (auto i : static_array)
	printf("%u", i);
printf("\n");
```
```D
// D
int[4] static_array = [0, 1, 2, 3,]; 
writef("%u\n\0", static_array[3]); 
foreach( i; static_array)
    writef("%u\0", i); 
writef("\n\0"); 
```
- Array initializer list is printed with Brackets 
- **range-base for-loop** are replaced by **foreach**. 
   - The **auto** keyword is not printed in **foreach**.

#### dynamic
```C++
// C++
void print_array(int* arr, size_t size){
	for (int i = 0; i < size; ++i)
		printf("%u", arr[i]);
	printf("\n");
}
int main(){
	int* dynamic_array = new int[5];
	dynamic_array[4] = 3;
	print_array(dynamic_array + 1, 4);
	delete[] dynamic_array;
}
```
```D
// D
void print_array(int[] arr, size_t size){
    for(int i = 0; i < size; ++i)
        writef("%u\0", arr[i]); 
    writef("\n\0");
}
int main(){ 
    int[] dynamic_array = new int[5]; 
    dynamic_array[4] = 3; 
    print_array(dynamic_array[1..$], 4); 
    dynamic_array = null; 
}
```
- pointer + 1 is replaced by pointer[1..$]
- **int*** is replaced by **int[]**

## How to install it?
1. Install **clang** : http://clang.llvm.org/get_started.html
2. Check out **CPP2D** from : https://github.com/lhamot/CPP2D.git
3. Run **cmake** in the root directory of **CPP2D**
   * Set the path to **LLVM**
4. Run **make**

## How to use it?
**Be aware than this project is far to be finished. Do not expect a fully working D project immediately. That you can expect is a great help to the conversion of your project, doing all the simple repetitive job, which is not so bad.**

There is many enhancement to do in the usage of CPP2D.

For the moment there is two way to use CPP2d:

### 1. Call file by file
1. Go to the destination directory (D project)
2. Call ```CPP2D.exe [options] <source0> [... <sourceN>] -- [compiler options]```
   - <sourceN> are C++ source files
   - [compiler options] are options forwarded to the compiler, like includes path, preprocessor definitions and so on.
   - [options] can be **-macro-stmt** and **-macro-exec** which are for macro handling

### 2. Use compilation database
I didn't tested it yet!

## Future of the project?
Small C++ project are almost fully convertible to D, but many things have to be done for the bigger ones.

Main missing features are:
- Better macro handling
- Namespace/Module handling
- External library handling, and specifically the stdlib
- Variadic template

## Want to help?
I would be happy to get some help on this project. If you are interested, do not hesitate to contact me at loic.hamot@yahoo.fr, or juste checkout and enjoy!
