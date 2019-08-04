#pragma once

#include <nano_pow/driver.hpp>

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

namespace nano_pow
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
		uint64_t threshold_get () const override;
		void threads_set (unsigned threads) override;
		size_t threads_get () const override;
		void memory_set (size_t memory) override;
		uint64_t solve (std::array<uint64_t, 2> nonce) override;
		void dump () const override;
	private:
		void fill_loop () const;
		uint64_t search_loop () const;
		bool error () const;
		opencl_environment environment;
		cl_context context { 0 };
		cl_program program { 0 };
		unsigned threads;
		uint64_t threshold;
		cl_mem slab { 0 };
		uint64_t slab_size;
		uint64_t slab_entries;
		cl_device_id selected_device;
		cl_kernel fill { 0 };
		cl_kernel search { 0 };
		cl_command_queue queue { 0 };
		cl_mem result_buffer { 0 };
		cl_mem nonce_buffer { 0 };
		uint32_t stepping { 256 };
	};
}
