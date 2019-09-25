#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <sstream>

#include <nano_pow/opencl_driver.hpp>
#include <nano_pow/pow.hpp>

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
	for (auto & platform : platforms)
	{
		stream << '\t' << platform.getInfo<CL_PLATFORM_PROFILE> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_VERSION> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_NAME> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_VENDOR> ()
		       << "\n\t" << platform.getInfo<CL_PLATFORM_EXTENSIONS> ()
		       << std::endl;

		std::vector<cl::Device> devices;
		try
		{
			(void)platform.getDevices (CL_DEVICE_TYPE_ALL, &devices);
		}
		catch (cl::Error const & err)
		{
			stream << "\nERROR\n" << to_string (err.err ()) << std::endl;
		}
		stream << "Number of devices: " << devices.size () << "\n";
		for (auto & device : devices)
		{
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
			stream << '\t' << device_type_string
			       << "\n\t" << device.getInfo<CL_DEVICE_NAME> ()
			       << "\n\t" << device.getInfo<CL_DEVICE_VENDOR> ()
			       << "\n\t" << device.getInfo<CL_DEVICE_PROFILE> ()
			       << "\n\t"
			       << "Compiler available: " << (device.getInfo<CL_DEVICE_COMPILER_AVAILABLE> () ? "true" : "false")
			       << "\n\t"
			       << "Global mem size: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE> () / (1024 * 1024) << " MB"
			       << "\n\t"
			       << "Max mem alloc size: " << device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE> () / (1024 * 1024) << " MB"
			       << "\n\t"
			       << "Compute units available: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS> ()
			       << std::endl;
		}
	}
}

nano_pow::opencl_driver::opencl_driver (unsigned short platform_id, unsigned short device_id, bool initialize)
{
	if (initialize)
	{
		std::vector<cl::Device> devices;
		// Platform
		if (platform_id >= environment.platforms.size ())
		{
			throw OCLDriverException (OCLDriverError::init, cl::Error(CL_INVALID_PLATFORM));
		}
		try
		{
			auto & platform (environment.platforms[platform_id]);
			(void)platform.getDevices (CL_DEVICE_TYPE_ALL, &devices);
		}
		catch (cl::Error const & err)
		{
			throw OCLDriverException (OCLDriverError::init, err);
		}
		// Device
		if (device_id >= devices.size ())
		{
			throw OCLDriverException (OCLDriverError::init, cl::Error(CL_DEVICE_NOT_FOUND));
		}
		try
		{
			selected_device = devices[device_id];
			auto platform_properties = selected_device.getInfo<CL_DEVICE_PLATFORM> ();
			global_mem_size = selected_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE> ();
			max_alloc_size = selected_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE> ();
			cl_context_properties contextProperties[3]{ CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platform_properties), 0 };
			context = cl::Context (selected_device, contextProperties);				
		}
		catch (cl::Error const & err)
		{
			throw OCLDriverException (OCLDriverError::init, err);
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
			throw OCLDriverException (OCLDriverError::build, err, details);
		}
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

bool nano_pow::opencl_driver::memory_set (size_t memory)
{
	assert (memory % sizeof (uint32_t) == 0);

	// The minimum max alloc size is defined in the OpenCL standard as 1/4 of the global memory size
	static unsigned constexpr max_slabs{ 4 };
	auto number_slabs = memory > max_alloc_size ? 4 : 1;

	slab_size = memory / number_slabs;
	slab_entries = memory / sizeof (uint32_t);

	if (verbose)
	{
		std::cout << "Memory set to " << number_slabs << " slab(s) of " << slab_size / (1024 * 1024) << "MB each" << std::endl;
	}
	try
	{
		slabs.clear ();
		fill_impl.setArg (0, slab_entries);
		search_impl.setArg (0, slab_entries);
		fill_impl.setArg (4, number_slabs);
		search_impl.setArg (4, number_slabs);

		for (auto i{ 0 }; i < number_slabs; ++i)
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
		throw OCLDriverException (OCLDriverError::memory_set, err);
	}
	return false;
}

void nano_pow::opencl_driver::fill ()
{
	uint64_t current (current_fill);
	uint32_t thread_count (this->threads);
	auto start = std::chrono::steady_clock::now ();
	try
	{
		while (current < current_fill + slab_entries)
		{
			fill_impl.setArg (3, static_cast<uint32_t> (current));
			queue.enqueueNDRangeKernel (fill_impl, 0, thread_count);
			current += thread_count * stepping;
		}
		current_fill += slab_entries;
		queue.finish ();
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverError::fill, err);
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
	auto max_current (0x0000FFFFFFFFFFFF - thread_count * stepping);
	try
	{
		while (result[1] == 0 && current <= max_current)
		{
			search_impl.setArg (3, (current & 0x0000FFFFFFFFFFFF)); // 48 bit solution part
			current += thread_count * stepping;
			queue.enqueueNDRangeKernel (search_impl, 0, thread_count);
			queue.enqueueReadBuffer (result_buffer, false, 0, sizeof (uint64_t) * 2, &result, nullptr, &events[0]);
			events[0].wait ();
			events[0] = events[1];
		}
	}
	catch (cl::Error const & err)
	{
		throw OCLDriverException (OCLDriverError::search, err);
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
		throw OCLDriverException (OCLDriverError::setup, err);
	}
	return nano_pow::driver::solve (nonce);
}

void nano_pow::opencl_driver::dump () const
{
	nano_pow::opencl_environment environment;
	environment.dump (std::cout);
}

bool nano_pow::opencl_driver::tune (unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & max_memory_a, size_t & best_memory_a, size_t & best_threads_a)
{
	using namespace std::chrono;
	auto megabytes = [](auto const memory) { return memory / (1024 * 1024); };

	size_t constexpr min_memory = (1ULL << 22) * 4;
	auto max_work_sizes = selected_device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES> ();
	size_t max_threads = 1;
	for (auto work_size : max_work_sizes)
	{
		max_threads *= work_size;
	}
	std::cerr << "Maximum device threads " << max_threads << std::endl;

	bool ok{ false };

	size_t memory (initial_memory_a);
	//TODO any device performing better with less than 2048 threads for a reasonable difficulty?
	size_t threads{ initial_threads_a };
	threads_set (threads);

	/*
	 * Find the maximum memory available
	 * Memory is allocated in up to 4 slabs due to contiguous memory allocation limits in some devices
	 * This result can vary depending on current usage of the device
	 */
	while (!ok)
	{
		try
		{
			memory_set (memory);
			fill ();
			//TODO do all implementations fail in fill? If not, uncomment next line
			// search ();
			ok = true;
		}
		catch (OCLDriverException const & err)
		{
			std::cerr << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			memory /= 2;
			if (memory < min_memory)
			{
				std::cerr << "Reached minimum memory " << megabytes (memory) << "MB" << std::endl;
				break;
			}
		}
	}
	if (ok)
	{
		std::cerr << "Found max memory " << megabytes (memory) << "MB" << std::endl;
		max_memory_a = memory;
	}

	/*
	 * Find the best memory configuration for this difficulty
	 */
	auto best_duration = std::chrono::system_clock::duration::max ().count ();
	while (ok)
	{
		try
		{
			auto start (steady_clock::now ());
			memory_set (memory);
			for (unsigned i{ 0 }; i < count_a; ++i)
			{
				solve ({ i, i });
			}
			auto duration = (steady_clock::now () - start).count ();
			std::cerr << threads << " threads " << megabytes(memory) << "MB TIME " << duration * 1e-6 / count_a << "ms" << std::endl;
			if (duration < best_duration)
			{
				best_duration = duration;
				memory /= 2;
			}
			else
			{
				memory *= 2;
				break;
			}
		}
		catch (OCLDriverException const & err)
		{
			std::cerr << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			ok = false;
		}
	}
	if (ok)
	{
		std::cerr << "Found best memory " << megabytes (memory) << "MB" << std::endl;
		best_memory_a = memory;
	}

	/*
	 * Find the best number of threads from a simple grid search
	 */
	threads *= 2;
	while (ok && threads <= max_threads)
	{
		try
		{
			auto start (steady_clock::now ());
			threads_set (threads);
			for (unsigned i{ 0 }; i < count_a; ++i)
			{
				solve ({ i, i });
			}
			auto duration = (steady_clock::now () - start).count ();
			std::cerr << threads << " threads " << megabytes (memory) << "MB TIME " << duration * 1e-6 / count_a << "ms" << std::endl;
			if (duration < best_duration)
			{
				best_duration = duration;
				threads *= 2;
			}
			else
			{
				threads /= 2;
				break;
			}
		}
		catch (OCLDriverException const & err)
		{
			std::cerr << threads << " threads, " << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			ok = false;
		}
	}
	if (ok)
	{
		std::cerr << "Found best threads " << threads << std::endl;
		best_threads_a = threads;
	}

	return ok;
}