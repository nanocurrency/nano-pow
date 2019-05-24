#pragma once

#include <iostream>
#include <vector>

#ifdef __APPLE__
#define CL_SILENCE_DEPRECATION
#include <OpenCL/opencl.h>
#else
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#endif

namespace opencl_pow_driver
{
	int main (int argc, char **argv, unsigned short platform_id = 0, unsigned short device_id = 0);

	class opencl_platform
	{
	public:
		cl_platform_id platform;
		std::vector<cl_device_id> devices;
	};

	class opencl_environment
	{
	public:
		opencl_environment (bool &);
		void dump (std::ostream & stream);
		std::vector<opencl_platform> platforms;
	};
}
