#pragma once

#include <ssp_pow/driver.hpp>

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

namespace ssp_pow
{
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
	class opencl_driver : public driver
	{
	public:
		opencl_driver (unsigned short platform_id = 0, unsigned short device_id = 0);
		~opencl_driver ();
		void threshold_set (uint64_t threshold) override;
		void threads_set (unsigned threads) override;
		void lookup_set (size_t lookup) override;
		virtual uint64_t solve (std::array<uint64_t, 2> nonce) override;
	private:
		opencl_environment environment;
		cl_context context { 0 };
		cl_program program { 0 };
		unsigned threads;
		unsigned threshold;
		unsigned short platform_id;
		unsigned short device_id;
		cl_mem slab { 0 };
		size_t lookup;
		cl_device_id selected_device;
	};
}
