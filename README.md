# CPP2D
(Clang based) C++ to D language converter

## Objective
The goal is to take a C++ project and convert all the source code to D language.

## Licence

Copyright Loïc HAMOT, 2016

Distributed under the Boost Software License, Version 1.0.

See ./LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt 

## Already handled C++ features

Majority of C++ code is already convertible to D.

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

Some samples here : https://github.com/lhamot/CPP2D/wiki/Conversion-samples

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

CPP2D work like any clang tools. This could help you:
- http://clang.llvm.org/docs/HowToSetupToolingForLLVM.html

There is two way to use CPP2D:

### 1. Without compilation database
1. Go to the destination directory (D project)
2. Call ```CPP2D.exe [options] <source0> [... <sourceN>] -- [compiler options]```
   - The two dashes are needed to informe cpp2d you don't need a compilation database
   - <sourceN> are C++ source files
   - [compiler options] are options forwarded to the compiler, like includes path, preprocessor definitions and so on.
   - [options] can be **-macro-stmt** and **-macro-exec** which are for macro handling

### 2. With compilation database

It seems to be impossible to generate a compilation database under windows...

If you are not under windows, and you are using cmake to compile your project:
- Add a line in your CMakeFiles.txt
```cmake
include_directories(path/to/llvm/lib/clang/3.9.0/include)
```
```sh
# Go to project build directory
$ cd project/build/directory
# Run cmake with "**cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON path/to/project/sources**"
$ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON path/to/project/sources
# You will obtain a compilation database
# Link it to the source project
$ ln -s $PWD/compile_commands.json path/to/project/source/
# Convert files, calling cpp2d
$ cd path/to/project/source
$ path/to/CPP2D/cpp2d source1.cpp source2.cpp source3.cpp
# You will find **D** files in the project/build/directory
```

Need for more documentation? You can search here : 
- http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools

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
