cmake_minimum_required(VERSION 3.13.4)
project(llvm-analysis)

#===============================================================================
# 1. LOAD LLVM CONFIGURATION
#===============================================================================
# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "")

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

# FIXME: This is a warkaround for #25. Remove once resolved and use
# find_package(LLVM 11.0.0 REQUIERED CONFIG) instead.
find_package(LLVM REQUIRED CONFIG)

# Includes headers from LLVM - update the include paths accordingly
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

#===============================================================================
# 2. BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 14 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

#===============================================================================
# 3. ADD THE TARGET
#===============================================================================
add_library(DominatorsAnalysis SHARED DominatorsAnalysis.cpp)
add_library(LivenessAnalysis SHARED LivenessAnalysis.cpp)

add_executable(analysis
  StaticMain.cpp
  DominatorsAnalysis.cpp
  LivenessAnalysis.cpp
)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(DominatorsAnalysis
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(LivenessAnalysis
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

target_link_libraries(analysis
  LLVMCore
  LLVMPasses
  LLVMIRReader
  LLVMSupport
  )