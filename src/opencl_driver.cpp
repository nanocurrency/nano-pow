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
				<< "\n\t"	<< "Global mem size: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()
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
	auto platform_properties = selected_device.getInfo< CL_DEVICE_PLATFORM> ();
	cl_context_properties contextProperties[3] { CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platform_properties), 0 };
	context = cl::Context (selected_device, contextProperties);
	std::vector<cl::Device> program_devices{ selected_device };
	try {
		program = cl::Program(context, nano_pow::opencl_program, false);
		program.build (program_devices, nullptr, nullptr);
		fill_impl = cl::Kernel(program, "fill");
		search_impl = cl::Kernel(program, "search");
		result_buffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint64_t));
		search_impl.setArg(0, result_buffer);
		nonce_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(uint64_t) * 2);
		search_impl.setArg(3, nonce_buffer);
		fill_impl.setArg(2, nonce_buffer);
		search_impl.setArg(4, stepping);
		fill_impl.setArg(3, stepping);
		queue = cl::CommandQueue(context, selected_device);
		queue.finish();
	}
	catch (cl::Error const & err) {
		auto details (program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(selected_device));
		throw OCLDriverException (err, OCLDriverError::build, details);
	}
}

void nano_pow::opencl_driver::difficulty_set (uint64_t difficulty_a)
{
	this->difficulty_inv = nano_pow::reverse (difficulty_a);
	this->difficulty = difficulty_a;
}

uint64_t nano_pow::opencl_driver::difficulty_get () const
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
	slab_size = memory;
	slab_entries = memory / sizeof (uint32_t);
	try	{
		slab = cl::Buffer(context, CL_MEM_READ_WRITE, slab_size);
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
	try {
		while (current < current_fill + slab_entries)
		{
			fill_impl.setArg(4, static_cast<uint32_t> (current));
			queue.enqueueNDRangeKernel(fill_impl, 0, thread_count);
			current += thread_count * stepping;
		}
		current_fill += slab_entries;
		queue.finish();
	}
	catch (cl::Error const& err) {
		throw OCLDriverException(err, OCLDriverError::fill);
	}
}

uint64_t nano_pow::opencl_driver::search ()
{
	std::array <cl::Event , 2 > events;

	uint64_t current (0);
	uint64_t result (0);

	uint32_t thread_count (this->threads);
	
	try {
		while (result == 0 && current < std::numeric_limits<uint32_t>::max())
		{
			search_impl.setArg(5, static_cast<uint32_t> (current));
			current += thread_count * stepping;
			queue.enqueueNDRangeKernel(search_impl, 0, thread_count);
			queue.enqueueReadBuffer(result_buffer, false, 0, sizeof(uint64_t), &result, nullptr, &events[0]);
			events[0].wait();
			events[0] = events[1];
		}
	}
	catch (cl::Error const& err) {
		throw OCLDriverException(err, OCLDriverError::search);
	}
	return result;
}

uint64_t nano_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	uint64_t result{ 0 };
	try {
		search_impl.setArg (1, slab);
		search_impl.setArg (2, slab_entries);
		search_impl.setArg (6, difficulty_inv);

		fill_impl.setArg (0, slab);
		fill_impl.setArg (1, slab_entries);

		queue.enqueueWriteBuffer (result_buffer, false, 0, sizeof (result), &result);
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
