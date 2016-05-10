cmake_minimum_required(VERSION 2.6)

project(cpp2d)

find_path(LLVM_PATH llvm
   PATH_SUFFIXES llvm
   DOC "Include directory for llvm"
   )
link_directories(${LLVM_PATH}/lib)
include_directories(${LLVM_PATH}/include)
include_directories(${LLVM_PATH}/tools/clang/include)

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
)

if(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti")
    target_link_libraries(cpp2d rt dl tinfo pthread z m)
endif()
