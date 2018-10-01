cmake_minimum_required(VERSION 2.6)

project(CPP2D_UT_CPP)

if(CMAKE_EXPORT_COMPILE_COMMANDS STREQUAL "ON")
    include_directories(${LLVM_LIB}/clang/4.0.1/include)
endif()

# Déclaration de l'exécutable
add_executable(
    CPP2D_UT_CPP
    framework.cpp
    main.cpp
    test.cpp
	stdlib_testsuite.cpp
	template_testsuite.cpp
	comment.cpp
)
if(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -fno-rtti")
endif()