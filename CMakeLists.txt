cmake_minimum_required (VERSION 3.4)
project (ssp-pow)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

include_directories (${CMAKE_SOURCE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)

find_package (Boost 1.67.0 REQUIRED COMPONENTS program_options)

include_directories (${Boost_INCLUDE_DIR})

add_library (siphash
	highwayhash/highwayhash/sip_hash.cc
	highwayhash/highwayhash/sip_hash.h)

include_directories (siphash PUBLIC highwayhash)

add_subdirectory (googletest/googletest)
include_directories (googletest/googletest/include)

add_executable (ssp_pow_driver
	cpp_driver.cpp
	cpp_driver.hpp
	gtest_driver.cpp
	include/ssp_pow/pow.hpp
	main.cpp)

target_link_libraries (ssp_pow_driver
	Boost::program_options
	gtest)
