#pragma once

#include <nano_pow/driver.hpp>
#include <nano_pow/opencl.hpp>

#include <iostream>
#include <vector>

namespace nano_pow
{
enum class OCLDriverError
{
	unknown = 0,
	init,
	build,
	setup,
	memory_set,
	fill,
	search
};
inline const char * to_string (OCLDriverError const err)
{
	switch (err)
	{
		case OCLDriverError::unknown:
			return "Unknown";
		case OCLDriverError::init:
			return "Init";
		case OCLDriverError::build:
			return "Build";
		case OCLDriverError::setup:
			return "Setup";
		case OCLDriverError::memory_set:
			return "Memory Set";
		case OCLDriverError::fill:
			return "Fill";
		case OCLDriverError::search:
			return "Search";
		default:
			return "Invalid";
	}
}

class OCLDriverException : public std::exception
{
private:
	cl::Error cl_err_{ CL_SUCCESS };
	const OCLDriverError err_origin_;
	const char * err_string_{ "" };
	const std::string err_details_;

public:
	OCLDriverException (const OCLDriverError origin, const cl::Error & cl_err = cl::Error (1), const std::string details = "") :
	cl_err_ (cl_err), err_origin_ (origin), err_string_ (to_string (cl_err.err ())), err_details_ (details)
	{
	}

	~OCLDriverException () throw ()
	{
	}

	virtual const char * what () const throw ()
	{
		return err_string_;
	}

	cl_int err (void) const
	{
		return cl_err_.err ();
	}

	const char * origin () const
	{
		return to_string (err_origin_);
	}

	void print (std::ostream & stream) const
	{
		stream << "During " << origin () << ": " << what () << " (" << err () << ")" << std::endl;
		if (!err_details_.empty ())
		{
			stream << err_details_ << std::endl;
		}
	}
};

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
	opencl_driver (unsigned short platform_id = 0, unsigned short device_id = 0, bool initialize = true);
	void initialize (unsigned short platform_id, unsigned short device_id);
	void difficulty_set (nano_pow::uint128_t difficulty_a) override;
	nano_pow::uint128_t difficulty_get () const override;
	void threads_set (unsigned threads) override;
	size_t threads_get () const override;
	bool memory_set (size_t memory) override;
	std::array<uint64_t, 2> solve (std::array<uint64_t, 2> nonce) override;
	void dump () const override;
	bool tune (unsigned const count, size_t const initial_memory, size_t const initial_threads, size_t & max_memory, size_t & best_memory, size_t & best_threads) override;

private:
	void fill () override;
	std::array<uint64_t, 2> search () override;
	opencl_environment environment;
	cl::Context context;
	cl::Program program;
	uint32_t threads{ 8192 };
	nano_pow::uint128_t difficulty;
	nano_pow::uint128_t difficulty_inv;
	std::vector<cl::Buffer> slabs{ 0 };
	uint64_t global_mem_size;
	uint64_t max_alloc_size;
	uint64_t slab_entries{ 0 };
	uint64_t slab_size;
	cl::Device selected_device;
	cl::Kernel fill_impl{ 0 };
	cl::Kernel search_impl{ 0 };
	cl::CommandQueue queue;
	cl::Buffer result_buffer{ 0 };
	cl::Buffer nonce_buffer{ 0 };
	uint32_t stepping{ 256 };
	uint32_t current_fill{ 0 };
};
}
