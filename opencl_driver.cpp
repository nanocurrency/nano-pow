#include <nano_pow/opencl_driver.hpp>

#include <nano_pow/pow.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <chrono>

std::string opencl_program = R"%%%(
static __constant ulong lhs_or_mask = ~(ulong)(LONG_MAX);
static __constant ulong rhs_and_mask = LONG_MAX;
typedef struct
{
	ulong values[2];
} nonce_t;
nonce_t lhs_nonce (nonce_t item_a)
{
	nonce_t result = item_a;
	result.values[0] |= lhs_or_mask;
	return result;
}
nonce_t rhs_nonce (nonce_t item_a)
{
	nonce_t result = item_a;
	result.values[0] &= rhs_and_mask;
	return result;
}

typedef ulong uint64_t;
typedef uint uint32_t;
typedef uchar uint8_t;

/*
 SipHash reference C implementation
 
 Written in 2012 by
 Jean-Philippe Aumasson <jeanphilippe.aumasson@gmail.com>
 Daniel J. Bernstein <djb@cr.yp.to>
 
 To the extent possible under law, the author(s) have dedicated all copyright
 and related and neighboring rights to this software to the public domain
 worldwide. This software is distributed without any warranty.
 
 You should have received a copy of the CC0 Public Domain Dedication along with
 this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

/* default: SipHash-2-4 */
#define cROUNDS 2
#define dROUNDS 4

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define U32TO8_LE(p, v)                                                        \
(p)[0] = (uint8_t)((v));                                                   \
(p)[1] = (uint8_t)((v) >> 8);                                              \
(p)[2] = (uint8_t)((v) >> 16);                                             \
(p)[3] = (uint8_t)((v) >> 24);

#define U64TO8_LE(p, v)                                                        \
U32TO8_LE((p), (uint32_t)((v)));                                           \
U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#define U8TO64_LE(p)                                                           \
(((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |                        \
((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |                 \
((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |                 \
((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))

#define SIPROUND                                                               \
do {                                                                       \
v0 += v1;                                                              \
v1 = ROTL(v1, 13);                                                     \
v1 ^= v0;                                                              \
v0 = ROTL(v0, 32);                                                     \
v2 += v3;                                                              \
v3 = ROTL(v3, 16);                                                     \
v3 ^= v2;                                                              \
v0 += v3;                                                              \
v3 = ROTL(v3, 21);                                                     \
v3 ^= v0;                                                              \
v2 += v1;                                                              \
v1 = ROTL(v1, 17);                                                     \
v1 ^= v2;                                                              \
v2 = ROTL(v2, 32);                                                     \
} while (0)

#ifdef DEBUG
#define TRACE                                                                  \
do {                                                                       \
printf("(%3d) v0 %08x %08x\n", (int)inlen, (uint32_t)(v0 >> 32),       \
(uint32_t)v0);                                                  \
printf("(%3d) v1 %08x %08x\n", (int)inlen, (uint32_t)(v1 >> 32),       \
(uint32_t)v1);                                                  \
printf("(%3d) v2 %08x %08x\n", (int)inlen, (uint32_t)(v2 >> 32),       \
(uint32_t)v2);                                                  \
printf("(%3d) v3 %08x %08x\n", (int)inlen, (uint32_t)(v3 >> 32),       \
(uint32_t)v3);                                                  \
} while (0)
#else
#define TRACE
#endif

int siphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
			uint8_t *out, const size_t outlen) {
	
	uint64_t v0 = 0x736f6d6570736575UL;
	uint64_t v1 = 0x646f72616e646f6dUL;
	uint64_t v2 = 0x6c7967656e657261UL;
	uint64_t v3 = 0x7465646279746573UL;
	uint64_t k0 = U8TO64_LE(k);
	uint64_t k1 = U8TO64_LE(k + 8);
	uint64_t m;
	int i;
	const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
	const int left = inlen & 7;
	uint64_t b = ((uint64_t)inlen) << 56;
	v3 ^= k1;
	v2 ^= k0;
	v1 ^= k1;
	v0 ^= k0;
	
	if (outlen == 16)
		v1 ^= 0xee;
	
	for (; in != end; in += 8) {
		m = U8TO64_LE(in);
		v3 ^= m;
		
		TRACE;
		for (i = 0; i < cROUNDS; ++i)
			SIPROUND;
		
		v0 ^= m;
	}
	
	switch (left) {
		case 7:
			b |= ((uint64_t)in[6]) << 48;
		case 6:
			b |= ((uint64_t)in[5]) << 40;
		case 5:
			b |= ((uint64_t)in[4]) << 32;
		case 4:
			b |= ((uint64_t)in[3]) << 24;
		case 3:
			b |= ((uint64_t)in[2]) << 16;
		case 2:
			b |= ((uint64_t)in[1]) << 8;
		case 1:
			b |= ((uint64_t)in[0]);
			break;
		case 0:
			break;
	}
	
	v3 ^= b;
	
	TRACE;
	for (i = 0; i < cROUNDS; ++i)
		SIPROUND;
	
	v0 ^= b;
	
	if (outlen == 16)
		v2 ^= 0xee;
	else
		v2 ^= 0xff;
	
	TRACE;
	for (i = 0; i < dROUNDS; ++i)
		SIPROUND;
	
	b = v0 ^ v1 ^ v2 ^ v3;
	U64TO8_LE(out, b);
	
	if (outlen == 8)
		return 0;
	
	v1 ^= 0xdd;
	
	TRACE;
	for (i = 0; i < dROUNDS; ++i)
		SIPROUND;
	
	b = v0 ^ v1 ^ v2 ^ v3;
	U64TO8_LE(out + 8, b);
	
	return 0;
}

static ulong slot (ulong const size_a, ulong const item_a)
{
	ulong mask = size_a - 1;
	return item_a & mask;
}

static ulong hash (nonce_t const nonce_a, ulong const item_a)
{
	ulong result;
	int code = siphash ((uchar *) &item_a, sizeof (item_a), (uchar *) nonce_a.values, (uchar *) &result, sizeof (result));
	return result;
}

static ulong H0 (nonce_t nonce_a, ulong const item_a)
{
	return hash (lhs_nonce (nonce_a), item_a);
}

static ulong H1 (nonce_t nonce_a, ulong const item_a)
{
	return hash (rhs_nonce (nonce_a), item_a);
}

static ulong difficulty_quick (ulong const sum_a, ulong const difficulty_inv_a)
{
	return sum_a & difficulty_inv_a;
}

static bool passes_quick (ulong const sum_a, ulong const difficulty_inv_a)
{
	bool passed = difficulty_quick (sum_a, difficulty_inv_a) == 0;
	return passed;
}

static ulong reverse (ulong const item_a)
{
	ulong result = item_a;
	result = ((result >>  1) & 0x5555555555555555) | ((result & 0x5555555555555555) <<  1);
	result = ((result >>  2) & 0x3333333333333333) | ((result & 0x3333333333333333) <<  2);
	result = ((result >>  4) & 0x0F0F0F0F0F0F0F0F) | ((result & 0x0F0F0F0F0F0F0F0F) <<  4);
	result = ((result >>  8) & 0x00FF00FF00FF00FF) | ((result & 0x00FF00FF00FF00FF) <<  8);
	result = ((result >> 16) & 0x0000FFFF0000FFFF) | ((result & 0x0000FFFF0000FFFF) << 16);
	result = ( result >> 32                      ) | ( result                       << 32);
	return result;
}

static bool passes_sum (ulong const sum_a, ulong threshold_a)
{
	bool passed = reverse (~sum_a) > threshold_a;
	return passed;
}

__kernel void search (__global ulong * result_a, __global uint * const slab_a, ulong const size_a, __global ulong * const nonce_a, uint const count, uint const begin, ulong const threshold_a)
{
	//printf ("[%lu] Search %llu %llu %llx\n", get_global_id (0), slab_a, size_a, threshold_a);
	bool incomplete = true;
	uint lhs, rhs;
	nonce_t nonce_l;
	nonce_l.values [0] = nonce_a [0];
	nonce_l.values [1] = nonce_a [1];
	for (uint current = begin + get_global_id (0) * count, end = current + count; incomplete && current < end; ++current)
	{
		rhs = current;
		ulong hash_l = H1 (nonce_l, rhs);
		lhs = slab_a [slot (size_a, 0 - hash_l)];
		ulong sum = H0 (nonce_l, lhs) + hash_l;
		//printf ("%lu %lx %lu %lx\n", lhs, hash_l, rhs, hash2);
		incomplete = !passes_quick (sum, threshold_a) || !passes_sum (sum, reverse (threshold_a));
	}
	if (!incomplete)
	{
		*result_a = (((ulong) (lhs)) << 32) | rhs;
	}
}

__kernel void fill (__global uint * const slab_a, ulong const size_a, __global ulong * const nonce_a, uint const count, uint const begin)
{
	nonce_t nonce_l;
	nonce_l.values [0] = nonce_a [0];
	nonce_l.values [1] = nonce_a [1];
	//printf ("[%lu] Fill %llu %llu\n", get_global_id (0), slab_a, size_a);
	for (uint current = begin + get_global_id (0) * count, end = current + count; current < end && current < size_a; ++current)
	{
		ulong slot_l = slot (size_a, H0 (nonce_l, current));
		//printf ("Writing %llu to %llu\n", current, slot_l);
		slab_a [slot_l] = current;
	}
}
)%%%";

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
	program = cl::Program (context, opencl_program, false);
	try {
		program.build (program_devices, nullptr, nullptr);
		fill = cl::Kernel(program, "fill");
		search = cl::Kernel(program, "search");
		result_buffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint64_t));
		search.setArg(0, result_buffer);
		nonce_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(uint64_t) * 2);
		search.setArg(3, nonce_buffer);
		fill.setArg(2, nonce_buffer);
		queue = cl::CommandQueue(context, selected_device);
		queue.finish();
		search.setArg(4, stepping);
		fill.setArg(3, stepping);
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
		throw OCLDriverException(err, OCLDriverError::search);
	}
	return false;
}

void nano_pow::opencl_driver::fill_loop ()
{
	auto current (0);
	uint32_t thread_count (this->threads);

	queue.finish ();
	do
	{
		fill.setArg(4, sizeof(uint32_t), &current);
		current += thread_count * stepping;
		queue.enqueueNDRangeKernel(fill, 0, thread_count);
	}
	while (current < slab_entries);
	queue.finish ();
}

uint64_t nano_pow::opencl_driver::search_loop ()
{
	std::array <cl::Event , 2 > events;

	uint32_t current (0);
	uint64_t result (0);

	uint32_t thread_count (this->threads);
	
	do
	{
		search.setArg (5, sizeof (uint32_t), &current);
		current += thread_count * stepping;
		queue.enqueueNDRangeKernel(search, 0, thread_count);
		queue.enqueueReadBuffer(result_buffer, false, 0, sizeof(uint64_t), &result, nullptr, &events[0]);
		events[0].wait ();
		events[0] = events[1];
	}
	while (result == 0);
	queue.finish ();
	return result;
}

uint64_t nano_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	uint64_t result{ 0 };

	try {
		search.setArg (1, slab);
		search.setArg (2, slab_entries);
		search.setArg (6, difficulty_inv);

		fill.setArg (0, slab);
		fill.setArg (1, slab_entries);

		queue.enqueueWriteBuffer (result_buffer, false, 0, sizeof (result), &result);
		queue.enqueueWriteBuffer (nonce_buffer, false, 0, sizeof (uint64_t) * 2, nonce.data ());
	}
	catch (cl::Error const & err) {
		throw OCLDriverException (err, OCLDriverError::setup);
	}

	while (!result)
	{
		try {
			fill_loop();
		}
		catch (cl::Error const & err) {
			throw OCLDriverException (err, OCLDriverError::fill);
		}

		try {
			result = search_loop ();
		}
		catch (cl::Error const & err) {
			throw OCLDriverException (err, OCLDriverError::search);
		}
	}
	return result;
}

void nano_pow::opencl_driver::dump () const
{
	nano_pow::opencl_environment environment;
	environment.dump (std::cout);
}
