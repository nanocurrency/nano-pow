#include <nano_pow/conversions.hpp>
#include <nano_pow/opencl_driver.hpp>
#include <nano_pow/pow.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <string>

namespace nano_pow
{
extern std::string opencl_program;
}

nano_pow::opencl_environment::opencl_environment ()
{
	(void)cl::Platform::get (&platforms);
}

void nano_pow::opencl_environment::dump (std::ostream & stream)
{
	stream << "OpenCL found " << platforms.size () << " platforms" << std::endl;
	unsigned plat{ 0 };
	for (auto & platform : platforms)
	{
		std::vector<cl::Device> devices;
		try
		{
			(void)platform.getDevices (CL_DEVICE_TYPE_ALL, &devices);
		}
		catch (cl::Error const & err)
		{
			stream << "\nERROR\n"
			       << to_string (err.err ()) << std::endl;
		}
		stream << "Platform " << plat++ << " : " << devices.size () << " devices\n";
		stream << '\t' << platform.getInfo<CL_PLATFORM_PROFILE> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_VERSION> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_NAME> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_VENDOR> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_EXTENSIONS> ()
		       << std::endl;
		unsigned dev{ 0 };
		for (auto & device : devices)
		{
			stream << "\tDevice " << dev++ << std::endl;
			auto device_type = device.getInfo<CL_DEVICE_TYPE> ();
			std::string device_type_string;
			switch (device_type)
			{
				case CL_DEVICE_TYPE_ACCELERATOR:
					device_type_string = "ACCELERATOR";
					break;
				case CL_DEVICE_TYPE_CPU:
					device_type_string = "CPU";
					break;
				case CL_DEVICE_TYPE_DEFAULT:
					device_type_string = "DEFAULT";
					break;
				case CL_DEVICE_TYPE_GPU:
					device_type_string = "GPU";
					break;
				default:
					device_type_string = "Unknown";
					break;
			}
			stream << "\t\t" << device_type_string
			       << "\n\t\t" << device.getInfo<CL_DEVICE_NAME> ()
			       << "\n\t\t" << device.getInfo<CL_DEVICE_VENDOR> ()
			       << "\n\t\t" << device.getInfo<CL_DEVICE_PROFILE> ()
			       << "\n\t\t"
			       << "Compiler available: " << (device.getInfo<CL_DEVICE_COMPILER_AVAILABLE> () ? "true" : "false")
			       << "\n\t\t"
			       << "Global mem size: " << nano_pow::to_megabytes (device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE> ()) << " MB"
			       << "\n\t\t"
			       << "Max mem alloc size: " << nano_pow::to_megabytes (device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE> ()) << " MB"
			       << "\n\t\t"
			       << "Compute units available: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS> ()
			       << std::endl;
		}
	}
}

nano_pow::opencl_driver::~opencl_driver ()
{
	cancel_current ();
}

nano_pow::opencl_driver::opencl_driver (unsigned short platform_id, unsigned short device_id, bool initialize)
{
	if (initialize)
	{
		this->initialize (platform_id, device_id);
	}
}

void nano_pow::opencl_driver::initialize (unsigned short platform_id, unsigned short device_id)
{
	std::vector<cl::Device> devices;
	// Platform
	if (platform_id >= environment.platforms.size ())
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::init, cl::Error (CL_INVALID_PLATFORM));
	}
	try
	{
		auto & platform (environment.platforms[platform_id]);
		(void)platform.getDevices (CL_DEVICE_TYPE_ALL, &devices);
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::init, err);
	}
	// Device
	if (device_id >= devices.size ())
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::init, cl::Error (CL_DEVICE_NOT_FOUND));
	}
	try
	{
		selected_device = devices[device_id];
		std::cout << selected_device.getInfo<CL_DEVICE_VENDOR> () << ": " << selected_device.getInfo<CL_DEVICE_NAME> () << std::endl;
		auto platform_properties = selected_device.getInfo<CL_DEVICE_PLATFORM> ();
		global_mem_size = selected_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE> ();
		max_alloc_size = selected_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE> ();
		cl_context_properties contextProperties[3]{ CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platform_properties), 0 };
		context = cl::Context (selected_device, contextProperties);
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::init, err);
	}
	// Program
	try
	{
		std::vector<cl::Device> program_devices{ selected_device };
		program = cl::Program (context, nano_pow::opencl_program, false);
		program.build (program_devices, nullptr, nullptr);
		fill_impl = cl::Kernel (program, "fill");
		search_impl = cl::Kernel (program, "search");
		result_buffer = cl::Buffer (context, CL_MEM_WRITE_ONLY, sizeof (uint64_t) * 2);
		search_impl.setArg (10, result_buffer);
		nonce_buffer = cl::Buffer (context, CL_MEM_READ_WRITE, sizeof (uint64_t) * 2);
		search_impl.setArg (1, nonce_buffer);
		fill_impl.setArg (1, nonce_buffer);
		search_impl.setArg (2, stepping);
		fill_impl.setArg (2, stepping);
		queue = cl::CommandQueue (context, selected_device);
		queue.finish ();
	}
	catch (cl::Error const & err)
	{
		auto details (program.getBuildInfo<CL_PROGRAM_BUILD_LOG> (selected_device));
		throw OCLDriverException (OCLDriverExceptionOrigin::build, err, details);
	}
}

void nano_pow::opencl_driver::difficulty_set (nano_pow::uint128_t difficulty_a)
{
	this->difficulty_inv = nano_pow::reverse (difficulty_a);
	this->difficulty = difficulty_a;
	this->search_impl.setArg (9, difficulty_inv);
}

nano_pow::uint128_t nano_pow::opencl_driver::difficulty_get () const
{
	return difficulty;
}

void nano_pow::opencl_driver::threads_set (unsigned threads)
{
	this->threads = threads;
}

size_t nano_pow::opencl_driver::threads_get () const
{
	return threads;
}

size_t nano_pow::opencl_driver::max_threads ()
{
	auto max_work_sizes = selected_device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES> ();
	size_t max_threads = 1;
	for (auto work_size : max_work_sizes)
	{
		max_threads *= work_size;
	}
	return max_threads;
}

bool nano_pow::opencl_driver::memory_set (size_t memory)
{
	assert (memory > 0);
	assert ((memory & (memory - 1)) == 0);
	assert (memory / nano_pow::entry_size <= nano_pow::lookup_to_entries (32)); // 16GB limit

	// The minimum max alloc size is defined in the OpenCL standard as 1/4 of the global memory size
	unsigned const number_slabs = memory > max_alloc_size ? 4 : 1;

	slab_size = memory / number_slabs;
	slab_entries = nano_pow::memory_to_entries (memory);

	if (verbose)
	{
		std::cout << "Memory set to " << number_slabs << " slab(s) of " << nano_pow::to_megabytes (slab_size) << "MB each" << std::endl;
	}
	try
	{
		memory_reset ();
		fill_impl.setArg (0, slab_entries);
		search_impl.setArg (0, slab_entries);
		fill_impl.setArg (4, number_slabs);
		search_impl.setArg (4, number_slabs);

		for (unsigned i{ 0 }; i < number_slabs; ++i)
		{
			slabs.emplace_back (cl::Buffer (context, CL_MEM_READ_WRITE, slab_size));
			search_impl.setArg (5 + i, slabs[i]);
			fill_impl.setArg (5 + i, slabs[i]);
		}
		for (auto i{ number_slabs }; i < max_slabs; ++i)
		{
			search_impl.setArg (5 + i, nullptr);
			fill_impl.setArg (5 + i, nullptr);
		}
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::memory_set, err);
	}
	return false;
}

void nano_pow::opencl_driver::memory_reset ()
{
	slabs.clear ();
	current_fill = 0;
}

void nano_pow::opencl_driver::fill ()
{
	uint64_t current (current_fill);
	uint32_t thread_count (this->threads);
	auto start = std::chrono::steady_clock::now ();
	try
	{
		while (!cancel && current < current_fill + slab_entries)
		{
			fill_impl.setArg (3, static_cast<uint32_t> (current));
			queue.enqueueNDRangeKernel (fill_impl, cl::NullRange, cl::NDRange (thread_count));
			current += thread_count * stepping;
		}
		current_fill += slab_entries;
		queue.finish ();
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::fill, err);
	}
	if (verbose)
	{
		std::cout << "Filled " << slab_entries << " entries in " << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now () - start).count () << " ms" << std::endl;
	}
}

std::array<uint64_t, 2> nano_pow::opencl_driver::search ()
{
	std::array<cl::Event, 2> events;
	uint64_t current (0);
	std::array<uint64_t, 2> result = { 0, 0 };
	uint32_t thread_count (this->threads);
	auto start = std::chrono::steady_clock::now ();
	size_t constexpr max_48bit{ (1ULL << 48) - 1 };
	auto const max_current (max_48bit - thread_count * stepping);
	try
	{
		while (!cancel && result[1] == 0 && current <= max_current)
		{
			search_impl.setArg (3, (current & max_48bit));
			current += thread_count * stepping;
			queue.enqueueNDRangeKernel (search_impl, cl::NullRange, cl::NDRange (thread_count));
			queue.enqueueReadBuffer (result_buffer, false, 0, sizeof (uint64_t) * 2, &result, nullptr, &events[0]);
			events[0].wait ();
			events[0] = events[1];
		}
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::search, err);
	}
	if (verbose)
	{
		std::cout << "Searched " << current << " nonces in " << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now () - start).count () << " ms" << std::endl;
	}
	return result;
}

std::array<uint64_t, 2> nano_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	std::array<uint64_t, 2> result = { 0, 0 };
	try
	{
		queue.enqueueWriteBuffer (result_buffer, false, 0, sizeof (uint64_t) * 2, &result);
		queue.enqueueWriteBuffer (nonce_buffer, false, 0, sizeof (uint64_t) * 2, nonce.data ());
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverExceptionOrigin::setup, err);
	}
	return nano_pow::driver::solve (nonce);
}

void nano_pow::opencl_driver::dump () const
{
	nano_pow::opencl_environment environment;
	environment.dump (std::cout);
}
