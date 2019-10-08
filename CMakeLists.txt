cmake_minimum_required (VERSION 3.4)
project (nano-pow)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set (NANO_POW_TEST OFF CACHE BOOL "")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

option(NANO_POW_ASAN_INT "Enable ASan+UBSan+Integer overflow" OFF)
option(NANO_POW_ASAN "Enable ASan+UBSan" OFF)
option(NANO_POW_TSAN "Enable TSan" OFF)

if (NOT WIN32)
	add_compile_options(-Wall -Wextra -pedantic)
endif ()

# Linker flags
if (APPLE)
	set (PLATFORM_LINK_FLAGS "-framework OpenCL")
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

# Sanitizer flags
if (WIN32)
	if (${NANO_POW_TSAN} OR ${NANO_POW_ASAN} OR ${NANO_POW_ASAN_INT})
		message (WARNING "Cannot use TSAN or ASAN on Windows, sanitizers ignored")
	endif()
else ()
	if (${NANO_POW_ASAN} OR ${NANO_POW_ASAN_INT})
		if (${NANO_POW_ASAN_INT})
			add_compile_options(-fsanitize=address,undefined,integer)
			set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined,integer")
		else ()
			add_compile_options(-fsanitize=address,undefined)
			set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined")
		endif()
	elseif (${NANO_POW_TSAN})
		add_compile_options(-fsanitize=thread)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=thread")
	endif()
endif ()

set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINK_FLAGS}" )

add_custom_command (
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/opencl_program.cpp
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/src/opencl_program.cl
	WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/src
	COMMAND ${CMAKE_COMMAND} -DCL_FILE=opencl_program.cl -DCPP_FILE=opencl_program.cpp -P "${CMAKE_CURRENT_SOURCE_DIR}/ConvertOpenCL.cmake"
	COMMENT "Generating OpenCL source file"
	VERBATIM
)

if (${NANO_POW_TEST})
	add_subdirectory (googletest/googletest)
	include_directories (googletest/googletest/include)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	SET (PLATFORM_SOURCE include/plat/win/memory.cpp)
else ()
	SET (PLATFORM_SOURCE include/plat/unix/memory.cpp)
endif ()

include_directories (cxxopts/include)

add_library (nano_pow
	${PLATFORM_SOURCE}
	include/nano_pow/conversions.hpp
	include/nano_pow/cpp_driver.hpp
	include/nano_pow/driver.hpp
	include/nano_pow/memory.hpp
	include/nano_pow/opencl.hpp
	include/nano_pow/opencl_driver.hpp
	include/nano_pow/plat.hpp
	include/nano_pow/pow.hpp
	include/nano_pow/tuning.hpp
	include/nano_pow/uint128.hpp
	include/nano_pow/xoroshiro128starstar.hpp

	src/cpp_driver.cpp
	src/driver.cpp
	src/opencl_driver.cpp
	src/opencl_program.cpp
	src/tuning.cpp
)

target_include_directories (nano_pow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (nano_pow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (${NANO_POW_TEST})
	add_executable (nano_pow_driver
		src/testing.cpp
		src/main.cpp
		cxxopts/include/cxxopts.hpp)

	target_link_libraries (nano_pow_driver
		gtest
		nano_pow
		${OpenCL_LIBRARY})
endif ()
