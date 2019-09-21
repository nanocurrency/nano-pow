#include <nano_pow/opencl_driver.hpp>

#include <nano_pow/pow.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <chrono>

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
	stream << "OpenCL found " << platforms.size() << " platforms" << std::endl;;
	for (auto & platform: platforms)
	{
		stream	<< '\t'	<< platform.getInfo<CL_PLATFORM_PROFILE> ()
			<< "\n\t"	<< platform.getInfo<CL_PLATFORM_VERSION> ()
			<< "\n\t"	<< platform.getInfo<CL_PLATFORM_NAME> ()
			<< "\n\t"	<< platform.getInfo<CL_PLATFORM_VENDOR> ()
			<< "\n\t"	<< platform.getInfo<CL_PLATFORM_EXTENSIONS> ()
			<< std::endl;

		std::vector<cl::Device> devices;
		(void)platform.getDevices (CL_DEVICE_TYPE_ALL, &devices);
		stream << "Number of devices: " << devices.size() << "\n";
		for (auto & device: devices)
		{
			auto device_type = device.getInfo<CL_DEVICE_TYPE>();
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
			stream << '\t'	<< device_type_string
				<< "\n\t"	<< device.getInfo<CL_DEVICE_NAME>()
				<< "\n\t"	<< device.getInfo<CL_DEVICE_VENDOR>()
				<< "\n\t"	<< device.getInfo<CL_DEVICE_PROFILE>()
				<< "\n\t"	<< "Compiler available: " << (device.getInfo<CL_DEVICE_COMPILER_AVAILABLE>() ? "true" : "false")
				<< "\n\t"	<< "Global mem size: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() / (1024 * 1024) << " MB"
				<< "\n\t"	<< "Max mem alloc size: " << device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() / (1024 * 1024) << " MB"
				<< "\n\t"	<< "Compute units available: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS> ()
				<< std::endl;
		}
	}
}

nano_pow::opencl_driver::opencl_driver (unsigned short platform_id, unsigned short device_id) :
threads (8192)
{
	auto & platform (environment.platforms[platform_id]);
	std::vector<cl::Device> devices;
	(void)platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
	selected_device = devices[device_id];
	auto platform_properties = selected_device.getInfo<CL_DEVICE_PLATFORM>();
	global_mem_size = selected_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
	max_alloc_size = selected_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
	cl_context_properties contextProperties[3] { CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platform_properties), 0 };
	context = cl::Context (selected_device, contextProperties);
	std::vector<cl::Device> program_devices{ selected_device };
	try {
		program = cl::Program(context, nano_pow::opencl_program, false);
		program.build (program_devices, nullptr, nullptr);
		fill_impl = cl::Kernel(program, "fill");
		search_impl = cl::Kernel(program, "search");
		result_buffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint64_t) * 2);
		search_impl.setArg(10, result_buffer);
		nonce_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(uint64_t) * 2);
		search_impl.setArg(1, nonce_buffer);
		fill_impl.setArg(1, nonce_buffer);
		search_impl.setArg(2, stepping);
		fill_impl.setArg(2, stepping);
		queue = cl::CommandQueue(context, selected_device);
		queue.finish();
	}
	catch (cl::Error const & err) {
		auto details (program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(selected_device));
		throw OCLDriverException (err, OCLDriverError::build, details);
	}
}

void nano_pow::opencl_driver::difficulty_set (nano_pow::uint128_t difficulty_a)
{
	this->difficulty_inv = nano_pow::reverse (difficulty_a);
	this->difficulty = difficulty_a;
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
	assert(memory % sizeof(uint32_t) == 0);

	// The minimum max alloc size is defined in the OpenCL standard as 1/4 of the global memory size
	static unsigned constexpr max_slabs{ 4 };
	auto number_slabs = memory > max_alloc_size ? 4 : 1;

	slab_size = memory / number_slabs;
	slab_entries = memory / sizeof(uint32_t);

	// std::cerr << number_slabs << "x " << slab_size / (1024 * 1024) << "MB " << std::endl;
	try	{
		fill_impl.setArg(0, slab_entries);
		search_impl.setArg(0, slab_entries);
		fill_impl.setArg(4, number_slabs);
		search_impl.setArg(4, number_slabs);

		for (auto i{ 0 }; i < number_slabs; ++i)
		{
			slabs.emplace_back(cl::Buffer(context, CL_MEM_READ_WRITE, slab_size));
			search_impl.setArg(5 + i, slabs[i]);
			fill_impl.setArg(5 + i, slabs[i]);
		}
		for (auto i{ number_slabs }; i < max_slabs; ++i)
		{
			search_impl.setArg(5 + i, nullptr);
			fill_impl.setArg(5 + i, nullptr);
		}
	}
	catch (cl::Error const & err) {
		throw OCLDriverException(err, OCLDriverError::memory_set);
	}
	return false;
}

void nano_pow::opencl_driver::fill ()
{
	uint64_t current (current_fill);
	uint32_t thread_count (this->threads);
	auto start = std::chrono::steady_clock::now ();
	try {
		while (current < current_fill + slab_entries)
		{
			fill_impl.setArg(3, static_cast<uint32_t> (current));
			queue.enqueueNDRangeKernel(fill_impl, 0, thread_count);
			current += thread_count * stepping;
		}
		current_fill += slab_entries;
		queue.finish();
	}
	catch (cl::Error const& err) {
		throw OCLDriverException(err, OCLDriverError::fill);
	}
	// std::cerr << "Fill took " << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - start).count() << " ms" << std::endl;
}

std::array<uint64_t, 2> nano_pow::opencl_driver::search ()
{
	std::array <cl::Event , 2 > events;
	uint64_t current (0);
	std::array<uint64_t, 2> result = { 0, 0 };
	uint32_t thread_count (this->threads);
	auto start = std::chrono::steady_clock::now();
	auto max ((std::numeric_limits<uint64_t>::max() >> 16) - thread_count * stepping);
	try {
		while (result[1] == 0 && current < max)
		{
			search_impl.setArg(3, ((current << 16) >> 16)); // 48 bit solution part
			current += thread_count * stepping;
			queue.enqueueNDRangeKernel(search_impl, 0, thread_count);
			queue.enqueueReadBuffer(result_buffer, false, 0, sizeof (uint64_t) * 2, &result, nullptr, &events[0]);
			events[0].wait();
			events[0] = events[1];
		}
	}
	catch (cl::Error const& err) {
		throw OCLDriverException(err, OCLDriverError::search);
	}
	// std::cerr << "Search took " << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - start).count() << " ms" << std::endl;
	return result;
}

std::array<uint64_t, 2> nano_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	std::array<uint64_t, 2> result = { 0, 0 };
	try {
		search_impl.setArg(9, difficulty_inv);
		queue.enqueueWriteBuffer (result_buffer, false, 0, sizeof (uint64_t) * 2, &result);
		queue.enqueueWriteBuffer (nonce_buffer, false, 0, sizeof (uint64_t) * 2, nonce.data ());
	}
	catch (cl::Error const & err) {
		throw OCLDriverException (err, OCLDriverError::setup);
	}
	return nano_pow::driver::solve (nonce);
}

void nano_pow::opencl_driver::dump () const
{
	nano_pow::opencl_environment environment;
	environment.dump (std::cout);
}
