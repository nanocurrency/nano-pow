#pragma once

#include <nano_pow/driver.hpp>

#include <iostream>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define __CL_ENABLE_EXCEPTIONS

#if defined(__APPLE__) || defined(__MACOSX)
#define CL_SILENCE_DEPRECATION
#include <OpenCL/opencl.hpp>
#else
#include <CL/cl.hpp>
#endif

namespace nano_pow
{
	class opencl_environment
	{
	public:
		opencl_environment ();
		void dump (std::ostream & stream);
		std::vector<cl::Platform> platforms;
	};
	class opencl_driver : public driver
	{
	public:
		opencl_driver (unsigned short platform_id = 0, unsigned short device_id = 0);
		void difficulty_set (uint64_t difficulty) override;
		uint64_t difficulty_get () const override;
		void threads_set (unsigned threads) override;
		size_t threads_get () const override;
		bool memory_set (size_t memory) override;
		bool solve (std::array<uint64_t, 2> nonce, uint64_t & result) override;
		bool ok () const override;
		void dump () const override;
	private:
		void fill_loop ();
		uint64_t search_loop ();
		opencl_environment environment;
		cl::Context context;
		cl::Program program;
		uint32_t threads;
		uint64_t difficulty;
		uint64_t difficulty_inv;
		cl::Buffer slab { 0 };
		uint64_t slab_size;
		uint64_t slab_entries;
		cl::Device selected_device;
		cl::Kernel fill { 0 };
		cl::Kernel search { 0 };
		cl::CommandQueue queue;
		cl::Buffer result_buffer { 0 };
		cl::Buffer nonce_buffer { 0 };
		uint32_t stepping { 256 };
		cl_int saved_error{ CL_SUCCESS };
	};
}
