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
		build,
		setup,
		memory_set,
		fill,
		search
	};
	inline const char* to_string (OCLDriverError const err)
	{
		switch (err)
		{
		case OCLDriverError::unknown: return "Unknown";
		case OCLDriverError::build: return "Build";
		case OCLDriverError::setup: return "Setup";
		case OCLDriverError::memory_set: return "Memory Set";
		case OCLDriverError::fill: return "Fill";
		case OCLDriverError::search : return "Search";
		default: return "Invalid";
		}
	}

	class OCLDriverException : public std::exception
	{
	private:
		cl::Error cl_err_{ CL_SUCCESS };
		const OCLDriverError err_origin_;
		const char* err_string_{""};
		const std::string err_details_;
	public:
		OCLDriverException (const cl::Error& cl_err, const OCLDriverError origin, const std::string details)
			: cl_err_ (cl_err), err_origin_ (origin), err_string_ (to_string (cl_err.err())), err_details_ (details) {}

		OCLDriverException(const cl::Error& cl_err, const OCLDriverError origin)
			: OCLDriverException(cl_err, origin, "") {}

		~OCLDriverException () throw() {}

		virtual const char* what () const throw ()
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
			if (!err_details_.empty())
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
		opencl_driver (unsigned short platform_id = 0, unsigned short device_id = 0);
		void difficulty_set (uint64_t difficulty) override;
		uint64_t difficulty_get () const override;
		void threads_set (unsigned threads) override;
		size_t threads_get () const override;
		bool memory_set (size_t memory) override;
		uint64_t solve (std::array<uint64_t, 2> nonce) override;
		void dump () const override;
	private:
		void fill () override;
		uint64_t search () override;
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
		cl::Kernel fill_impl { 0 };
		cl::Kernel search_impl { 0 };
		cl::CommandQueue queue;
		cl::Buffer result_buffer { 0 };
		cl::Buffer nonce_buffer { 0 };
		uint32_t stepping { 256 };
		uint32_t current_fill { 0 };
	};
}
