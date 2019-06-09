#pragma once

#include <iostream>
#include <vector>

#ifdef __APPLE__
#define CL_SILENCE_DEPRECATION
#include <OpenCL/opencl.h>
#else
#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#endif

#include <boost/program_options.hpp>

namespace opencl_pow_driver
{
	int main (boost::program_options::variables_map & vm, unsigned difficulty, unsigned lookup, unsigned count, unsigned short platform_id = 0, unsigned short device_id = 0);

	class opencl_platform
	{
	public:
		cl_platform_id platform;
		std::vector<cl_device_id> devices;
	};

	class opencl_environment
	{
	public:
		opencl_environment ();
		void dump (std::ostream & stream);
		std::vector<opencl_platform> platforms;
	};
}
