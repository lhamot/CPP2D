cmake_minimum_required(VERSION 2.6)

project(cpp2d)

find_path(LLVM_INCLUDE llvm
   PATH_SUFFIXES llvm
   DOC "Include directory for llvm"
   )
include_directories(${LLVM_INCLUDE})

find_path(LLVM_LIB llvm
   PATH_SUFFIXES llvm
   DOC "Lib directory for llvm"
)
link_directories(${LLVM_LIB})

find_path(CLANG_INCLUDE clang
   PATH_SUFFIXES clang
   DOC "Include directory for clang"
)
include_directories(${CLANG_INCLUDE})

find_path(CLANG_LIB clang
   PATH_SUFFIXES clang
   DOC "Lib directory for clang"
)
link_directories(${CLANG_LIB})

# Déclaration de l'exécutable
add_executable(
    cpp2d
    CPP2D.cpp
    CPP2DConsumer.cpp
    CPP2DFrontendAction.cpp
    CPP2DPPHandling.cpp
    CPP2DTools.cpp
    DPrinter.cpp
    MatchContainer.cpp
    CustomPrinters.cpp
	Options.cpp
    CustomPrinters/boost_port.cpp
    CustomPrinters/c_stdlib_port.cpp
    CustomPrinters/cpp_stdlib_port.cpp
)

target_link_libraries(cpp2d
  clangFrontend
  clangSerialization
  clangDriver
  clangParse
  clangSema
  clangAnalysis
  clangAST
  clangBasic
  clangEdit
  clangLex
  clangTooling
  clangASTMatchers
)

target_link_libraries(cpp2d
  LLVMX86AsmParser # MC, MCParser, Support, X86Desc, X86Info
  LLVMX86Desc # MC, Support, X86AsmPrinter, X86Info
  LLVMX86AsmPrinter # MC, Support, X86Utils
  LLVMX86Info # MC, Support, Target
  LLVMX86Utils # Core, Support
  LLVMipo
  LLVMScalarOpts
  LLVMInstCombine
  LLVMTransformUtils
  LLVMAnalysis
  LLVMTarget
  LLVMOption # Support
  LLVMMCParser # MC, Support
  LLVMMC # Object, Support
  LLVMObject # BitReader, Core, Support
  LLVMBitReader # Core, Support
  LLVMCore # Support
  LLVMProfileData
  LLVMSupport
  LLVMDemangle
)

if(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti")
    target_link_libraries(cpp2d rt dl tinfo pthread z m)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	target_link_libraries(cpp2d
		version.lib
	)
endif()

