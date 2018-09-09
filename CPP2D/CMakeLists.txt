cmake_minimum_required(VERSION 2.6)

project(cpp2d)

# Executable declaration
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
  LLVMBinaryFormat  # Needed under windows
)

if(CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(cpp2d rt dl tinfo pthread z m)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	target_link_libraries(cpp2d
		version.lib
	)
endif()
