cmake_minimum_required (VERSION 3.4)
project (nano-pow)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set (NANO_POW_TEST OFF CACHE BOOL "")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# add_compile_options(-Wall -Wextra -pedantic)

include_directories (${CMAKE_SOURCE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/highwayhash/highwayhash)

if (APPLE)
	set (PLATFORM_LINK_FLAGS "-framework OpenCL")
	SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINK_FLAGS}" )
else ()
	if (WIN32)
		set (PLATFORM_LINK_FLAGS "")
	else ()
		set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif ()
	find_package (OpenCL REQUIRED)
	include_directories (${OpenCL_INCLUDE_DIRS})
	link_directories (${OpenCL_LIBRARY})
endif ()

add_library (siphash
	highwayhash/highwayhash/sip_hash.cc
	highwayhash/highwayhash/sip_hash.h)

include_directories (siphash PUBLIC highwayhash)

if (${NANO_POW_TEST})
	add_subdirectory (googletest/googletest)
	include_directories (googletest/googletest/include)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	SET (PLATFORM_SOURCE include/plat/win/mman.c include/plat/win/mman.h)
endif ()

include_directories (cxxopts/include)

add_library (nano_pow
	${PLATFORM_SOURCE}
	cpp_driver.cpp
	cpp_driver.hpp
	include/nano_pow/driver.hpp
	include/nano_pow/pow.hpp
	opencl_driver.cpp
	opencl_driver.hpp
)

if (${NANO_POW_TEST})
	add_executable (nano_pow_driver
		gtest_driver.cpp
		main.cpp
		cxxopts/include/cxxopts.hpp)

	target_link_libraries (nano_pow_driver
		gtest
		nano_pow
		${OpenCL_LIBRARY})
endif ()
