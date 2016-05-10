cmake_minimum_required(VERSION 2.6)

project(CPP2D_UT_CPP)

# Déclaration de l'exécutable
add_executable(
    CPP2D_UT_CPP
    framework.cpp
    main.cpp
    test.cpp
)
if(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti")
endif()