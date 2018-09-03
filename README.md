# CPP2D
(Clang based) C++ to D language converter

## Objective
The goal is to take a C++ project and convert all the source code to D language.

## License

Copyright Loïc HAMOT, 2016

Distributed under the Boost Software License, Version 1.0.

See ./LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt

## Continuous integration

* Ubuntu

[![Build Status](https://travis-ci.org/lhamot/CPP2D.svg?branch=master)](https://travis-ci.org/lhamot/CPP2D)

* Windows

[![Build Status](https://ci.appveyor.com/api/projects/status/github/lhamot/cpp2d?svg=true)](https://ci.appveyor.com/project/lhamot/cpp2d)

## Already handled C++ features

Majority of C++ code is already convertible to D.

A not exhaustive list:
* class
   * constructor/destructor
   * virtual
   * abstract
   * override
   * initialization list
   * call base constructor
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
   * std::vector (partially)
   * std::array (partially)

Some samples here : https://github.com/lhamot/CPP2D/wiki/Conversion-samples

## Requirements
* cmake >= 2.6
* Tested with **gcc** 4.8.4 (**Ubuntu** 14.04.3) and **Visual Studio** 2015 (**Windows** 7 & 10)
* Tested with **LLVM/clang** **4.0.1**

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

CPP2D work like any clang tools. This could help you:
- http://clang.llvm.org/docs/HowToSetupToolingForLLVM.html

There is two way to use CPP2D:

### 1. Without compilation database
1. Go to the destination directory (D project)
2. Call ```CPP2D.exe [options] <source0> [... <sourceN>] -- [compiler options]```
   - The double dashes are needed to inform cpp2d you don't need a compilation database
   - ```<sourceN>``` are C++ source files
   - [compiler options] are options forwarded to the compiler, like includes path, preprocessor definitions and so on.
   - [options] can be **-macro-stmt** and **-macro-exec** which are for macro handling

### 2. With compilation database
It seems to be impossible to generate a compilation database under windows...

If you are not under windows, and you are using cmake to compile your project:
- Add this code in your(s) CMakeFiles.txt
```cmake
if(CMAKE_EXPORT_COMPILE_COMMANDS STREQUAL "ON")
    include_directories(${LLVM_PATH}/lib/clang/3.9.0/include)
endif()
```
```sh
# Go to project build directory
$ cd project/build/directory
# Run cmake, asking it to generate a compilation database
$ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON path/to/project/sources
# You will obtain a compilation database
# Link it to the source project
$ ln -s $PWD/compile_commands.json path/to/project/source/
# Convert files, calling cpp2d
$ cd path/to/project/source
$ project/build/directory/CPP2D/cpp2d source1.cpp source2.cpp source3.cpp
# You will find D files in the project/build/directory
# You also need to copy the cpp_std.d file to your D source directory
$ cp project/build/directory/CPP2D/cpp_std.d dproject/source
```

Need for more documentation? You can search here :
- http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools

## Future of the project?
Small C++ project are almost fully convertible to **D**, but many things have to be done for the bigger ones.

### Main missing features are:
* Better macro handling
* Namespace/Module handling
* External library handling, and specifically the stdlib
* Variadic template
   * Not tried to do yet
* Better conservation of comments
* Porting constness
   * Hard because **D** containers seem to not be const correct
* Handling const ref function argument
   * Hard because, unlike in **C++**, in **D** we can't pass a rvalue to a const ref parameter

### Other possible enhancements

* CMake : Create a cmake to compile a project linked to **LLVM**/**clang** in a cross-platform way seems to be not straightforward. If you can enhance my CMakeLists, please do it!
* Clang integration : Maybe there is a better way to integrate **CPP2D** with **clang**, like make it a real clang tool, in clang/tools, or clang/tools/extra.

## Want to help?
I would be happy to get some help on this project. If you are interested, do not hesitate to :
- Reach us on the mailing list: https://groups.google.com/forum/#!forum/cpp2d
- Contact me at loic.hamot@yahoo.fr
- Or just checkout and enjoy ;)
