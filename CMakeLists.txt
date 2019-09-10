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

file(READ src/opencl_program.cl opencl_program)
file(WRITE src/opencl_program.cpp "#include <string>\n"
"namespace nano_pow\n"
"{\n"
"std::string opencl_program = R\"OCL(\n"
"${opencl_program}\n"
")OCL\";\n"
"}")

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
	include/nano_pow/pow.hpp
	include/nano_pow/driver.hpp
	include/nano_pow/cpp_driver.hpp
	include/nano_pow/opencl.hpp
	include/nano_pow/opencl_driver.hpp
	src/cpp_driver.cpp
	src/opencl_driver.cpp
	src/opencl_program.cpp
)

target_include_directories (nano_pow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (nano_pow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (${NANO_POW_TEST})
	add_executable (nano_pow_driver
		src/gtest_driver.cpp
		src/main.cpp
		cxxopts/include/cxxopts.hpp)

	target_link_libraries (nano_pow_driver
		gtest
		nano_pow
		${OpenCL_LIBRARY})
endif ()
