cmake_minimum_required (VERSION 3.4)
project (nano-pow)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

include_directories (${CMAKE_SOURCE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)

set(Boost_USE_STATIC_LIBS	ON)
set(Boost_USE_MULTITHREAD	ON)

if (BOOST_ROOT)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")
endif ()

find_package (Boost 1.65.0 REQUIRED COMPONENTS program_options)

include_directories (${Boost_INCLUDE_DIR})

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

add_subdirectory (googletest/googletest)
include_directories (googletest/googletest/include)

add_library (blake2
	BLAKE2/sse/blake2-config.h
	BLAKE2/sse/blake2-impl.h
	BLAKE2/sse/blake2.h
	BLAKE2/sse/blake2b.c)

target_compile_definitions(blake2 PRIVATE -D__SSE2__)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	SET (PLATFORM_SOURCE include/plat/win/mman.c include/plat/win/mman.h)
endif ()

add_executable (nano_pow_driver
	${PLATFORM_SOURCE}
	cpp_driver.cpp
	cpp_driver.hpp
	gtest_driver.cpp
	include/nano_pow/driver.hpp
	include/nano_pow/hash.hpp
	include/nano_pow/pow.hpp
	main.cpp
	opencl_driver.cpp
	opencl_driver.hpp)

target_link_libraries (nano_pow_driver
	blake2
	${Boost_LIBRARIES}
	gtest
	${OpenCL_LIBRARY})
