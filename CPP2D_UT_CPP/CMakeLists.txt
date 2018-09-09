cmake_minimum_required(VERSION 2.6)

project(CPP2D_UT_CPP)

if(CMAKE_EXPORT_COMPILE_COMMANDS STREQUAL "ON")
    include_directories(${LLVM_LIBRARY_DIRS}/clang/6.0.1/include)
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
