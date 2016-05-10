# CPP2D
(Clang based) C++ to D language converter

## Objective
The goal is to take a C++ project and convert all the source code to D language.

## Licence

Copyright Loïc HAMOT, 2016

Distributed under the Boost Software License, Version 1.0.

See ./LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt 

## Already handled C++ features

Majority of C++ code is already convertible to C++.

A not exhaustive list:
* class
   * constructor/destructor
   * virtual
   * abstract
   * override
   * inisialisation list
   * call base constructore
* Operator overloading
   * member
   * free
      * left or right
* Template
   * function
   * function specialization
   * class
   * class/struct specialization
   * class/struct partial specialization
* arrays
   * static
   * dynamic
   * std::vector (partialy)
   * std::array (partialy)

Some samples heres : https://github.com/lhamot/CPP2D/wiki/Conversion-samples

## Requierements
* cmake >= 2.6
* Tested with **gcc** 4.9.2 (**Ubuntu** 15.04) and **Visual Studio** 2015 (**Windows** seven)
* Tested with **clang** r268594 and **llvm** r268845

## How to install it?
1. Install **clang** : http://clang.llvm.org/get_started.html
2. Check out **CPP2D** from : https://github.com/lhamot/CPP2D.git
3. Run **cmake** in the root directory of **CPP2D**
   1. Set the build type if needed (Debug;Release;MinSizeRel;RelWithDebInfo)
   2. Set the path to **LLVM** named **LLVM_PATH**.
   4. Generate
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

Waiting a better documentation you can see here : http://clang.llvm.org/docs/HowToSetupToolingForLLVM.html

## Future of the project?
Small C++ project are almost fully convertible to D, but many things have to be done for the bigger ones.

### Main missing features are:
* Better macro handling
* Namespace/Module handling
* External library handling, and specifically the stdlib
* Variadic template
   * Not tried to do yet
* Better conservation of comments
* Porting constness 
   * Hard because D containers seem to not be const correct
* Handling const ref function argument
   * Hard because, unlike in C++, in D we can't pass a rvalue to a const ref parameter

### Other possible enhancements

* CMake : Create a cmake to compile a project linked to LLVM/clang in a cross-platform way seems to be not straightforward. If you can enhance my CMakeLists, please do it!
* Clang integration : Maybe there is a better way to integrate CPP2D with clang, like make it a real clang tool, in clang/tools, or clang/tools/extra.
   
## Want to help?
I would be happy to get some help on this project. If you are interested, do not hesitate to contact me at loic.hamot@yahoo.fr, or juste checkout and enjoy!
