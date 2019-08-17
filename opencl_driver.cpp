#include <nano_pow/opencl_driver.hpp>

#include <nano_pow/pow.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <chrono>

std::string opencl_program = R"%%%(
typedef struct
{
	ulong values[2];
} nonce_t;
static __constant ulong lhs_or_mask = 0x1UL << 63 ;
static __constant ulong rhs_and_mask = 0xffffffff;
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

typedef ulong u64;
typedef uint u32;
typedef uchar u8;

#define ROTL(x,b) (u64)( ((x) << (b)) | ( (x) >> (64 - (b))) )

#define U32TO8_LE(p, v)         \
(p)[0] = (u8)((v)      ); (p)[1] = (u8)((v) >>  8); \
(p)[2] = (u8)((v) >> 16); (p)[3] = (u8)((v) >> 24);

#define U64TO8_LE(p, v)         \
U32TO8_LE((p),     (u32)((v)      ));   \
U32TO8_LE((p) + 4, (u32)((v) >> 32));

#define U8TO64_LE(p) \
(((u64)((p)[0])      ) | \
((u64)((p)[1]) <<  8) | \
((u64)((p)[2]) << 16) | \
((u64)((p)[3]) << 24) | \
((u64)((p)[4]) << 32) | \
((u64)((p)[5]) << 40) | \
((u64)((p)[6]) << 48) | \
((u64)((p)[7]) << 56))

#define SIPROUND            \
do {              \
v0 += v1; v1=ROTL(v1,13); v1 ^= v0; v0=ROTL(v0,32); \
v2 += v3; v3=ROTL(v3,16); v3 ^= v2;     \
v0 += v3; v3=ROTL(v3,21); v3 ^= v0;     \
v2 += v1; v1=ROTL(v1,17); v1 ^= v2; v2=ROTL(v2,32); \
} while(0)

/* SipHash-2-4 */
int crypto_auth( uchar *out, const uchar *in, ulong inlen, nonce_t nonce_a )
{
	/* "somepseudorandomlygeneratedbytes" */
	u64 v0 = 0x736f6d6570736575UL;
	u64 v1 = 0x646f72616e646f6dUL;
	u64 v2 = 0x6c7967656e657261UL;
	u64 v3 = 0x7465646279746573UL;
	u64 b;
	u64 k0 = nonce_a.values[0];
	u64 k1 = nonce_a.values[1];
	u64 m;
	const u8 *end = in + inlen - ( inlen % sizeof( u64 ) );
	const int left = inlen & 7;
	b = ( ( u64 )inlen ) << 56;
	v3 ^= k1;
	v2 ^= k0;
	v1 ^= k1;
	v0 ^= k0;
	
	for ( ; in != end; in += 8 )
	{
		m = U8TO64_LE( in );
#ifdef DEBUG
		printf( "(%3d) v0 %08x %08x\n", ( int )inlen, ( u32 )( v0 >> 32 ), ( u32 )v0 );
		printf( "(%3d) v1 %08x %08x\n", ( int )inlen, ( u32 )( v1 >> 32 ), ( u32 )v1 );
		printf( "(%3d) v2 %08x %08x\n", ( int )inlen, ( u32 )( v2 >> 32 ), ( u32 )v2 );
		printf( "(%3d) v3 %08x %08x\n", ( int )inlen, ( u32 )( v3 >> 32 ), ( u32 )v3 );
		printf( "(%3d) compress %08x %08x\n", ( int )inlen, ( u32 )( m >> 32 ), ( u32 )m );
#endif
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
	}
	
	switch( left )
	{
		case 7: b |= ( ( u64 )in[ 6] )  << 48;
			
		case 6: b |= ( ( u64 )in[ 5] )  << 40;
			
		case 5: b |= ( ( u64 )in[ 4] )  << 32;
			
		case 4: b |= ( ( u64 )in[ 3] )  << 24;
			
		case 3: b |= ( ( u64 )in[ 2] )  << 16;
			
		case 2: b |= ( ( u64 )in[ 1] )  <<  8;
			
		case 1: b |= ( ( u64 )in[ 0] ); break;
			
		case 0: break;
	}
	
#ifdef DEBUG
	printf( "(%3d) v0 %08x %08x\n", ( int )inlen, ( u32 )( v0 >> 32 ), ( u32 )v0 );
	printf( "(%3d) v1 %08x %08x\n", ( int )inlen, ( u32 )( v1 >> 32 ), ( u32 )v1 );
	printf( "(%3d) v2 %08x %08x\n", ( int )inlen, ( u32 )( v2 >> 32 ), ( u32 )v2 );
	printf( "(%3d) v3 %08x %08x\n", ( int )inlen, ( u32 )( v3 >> 32 ), ( u32 )v3 );
	printf( "(%3d) padding   %08x %08x\n", ( int )inlen, ( u32 )( b >> 32 ), ( u32 )b );
#endif
	v3 ^= b;
	SIPROUND;
	SIPROUND;
	v0 ^= b;
#ifdef DEBUG
	printf( "(%3d) v0 %08x %08x\n", ( int )inlen, ( u32 )( v0 >> 32 ), ( u32 )v0 );
	printf( "(%3d) v1 %08x %08x\n", ( int )inlen, ( u32 )( v1 >> 32 ), ( u32 )v1 );
	printf( "(%3d) v2 %08x %08x\n", ( int )inlen, ( u32 )( v2 >> 32 ), ( u32 )v2 );
	printf( "(%3d) v3 %08x %08x\n", ( int )inlen, ( u32 )( v3 >> 32 ), ( u32 )v3 );
#endif
	v2 ^= 0xff;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	b = v0 ^ v1 ^ v2  ^ v3;
	U64TO8_LE( out, b );
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
	int code = crypto_auth ((uchar *) &result, (uchar *) &item_a, sizeof (item_a), nonce_a);
	return result;
}

static ulong H0 (nonce_t nonce_a, ulong const item_a)
{
	nonce_t nonce = nonce_a;
	nonce.values [0] = nonce_a.values [0] | lhs_or_mask;
	nonce.values [1] = nonce_a.values [1];
	return hash (nonce, item_a);
}

static ulong H1 (nonce_t nonce_a, ulong const item_a)
{
	nonce_t nonce = nonce_a;
	nonce.values [0] = nonce_a.values [0] & rhs_and_mask;
	nonce.values [1] = nonce_a.values [1];
	return hash (nonce, item_a);
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
		//printf ("%lu %lx %lu %lu %lx\n", lhs, hash_l, slot_l, rhs, hash2);
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
	cl_uint platformIdCount = 0;
	clGetPlatformIDs (0, nullptr, &platformIdCount);
	std::vector<cl_platform_id> platformIds (platformIdCount);
	clGetPlatformIDs (platformIdCount, platformIds.data (), nullptr);
	for (auto i (platformIds.begin ()), n (platformIds.end ()); i != n; ++i)
	{
		opencl_platform platform;
		platform.platform = *i;
		cl_uint deviceIdCount = 0;
		clGetDeviceIDs (*i, CL_DEVICE_TYPE_ALL, 0, nullptr, &deviceIdCount);
		std::vector<cl_device_id> deviceIds (deviceIdCount);
		clGetDeviceIDs (*i, CL_DEVICE_TYPE_ALL, deviceIdCount, deviceIds.data (), nullptr);
		for (auto j (deviceIds.begin ()), m (deviceIds.end ()); j != m; ++j)
		{
			platform.devices.push_back (*j);
		}
		platforms.push_back (platform);
	}
}

void nano_pow::opencl_environment::dump (std::ostream & stream)
{
	auto index (0);
	size_t device_count (0);
	for (auto & i : platforms)
	{
		device_count += i.devices.size ();
	}
	stream << "OpenCL found " << platforms.size () << " platforms and " << device_count << " devices\n";
	for (auto i (platforms.begin ()), n (platforms.end ()); i != n; ++i, ++index)
	{
		std::vector<unsigned> queries = { CL_PLATFORM_PROFILE, CL_PLATFORM_VERSION, CL_PLATFORM_NAME, CL_PLATFORM_VENDOR, CL_PLATFORM_EXTENSIONS };
		stream << "Platform: " << index << std::endl;
		for (auto j (queries.begin ()), m (queries.end ()); j != m; ++j)
		{
			size_t platformInfoCount = 0;
			clGetPlatformInfo (i->platform, *j, 0, nullptr, &platformInfoCount);
			std::vector<char> info (platformInfoCount);
			clGetPlatformInfo (i->platform, *j, info.size (), info.data (), nullptr);
			stream << info.data () << std::endl;
		}
		for (auto j (i->devices.begin ()), m (i->devices.end ()); j != m; ++j)
		{
			std::vector<unsigned> queries = { CL_DEVICE_NAME, CL_DEVICE_VENDOR, CL_DEVICE_PROFILE };
			stream << "Device: " << j - i->devices.begin () << std::endl;
			for (auto k (queries.begin ()), o (queries.end ()); k != o; ++k)
			{
				size_t platformInfoCount = 0;
				clGetDeviceInfo (*j, *k, 0, nullptr, &platformInfoCount);
				std::vector<char> info (platformInfoCount);
				clGetDeviceInfo (*j, *k, info.size (), info.data (), nullptr);
				stream << '\t' << info.data () << std::endl;
			}
			cl_device_type deviceType;
			clGetDeviceInfo (*j, CL_DEVICE_TYPE, sizeof(deviceType), &deviceType, nullptr);
			std::string device_type_string;
			switch (deviceType)
			{
				case CL_DEVICE_TYPE_ACCELERATOR:
					device_type_string = "ACCELERATOR";
					break;
				case CL_DEVICE_TYPE_CPU:
					device_type_string = "CPU";
					break;
//				case CL_DEVICE_TYPE_CUSTOM:
//					device_type_string = "CUSTOM";
//					break;
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
			stream << '\t' << device_type_string << std::endl;

			cl_bool compilerAvailable;
			clGetDeviceInfo (*j, CL_DEVICE_COMPILER_AVAILABLE, sizeof(compilerAvailable), &compilerAvailable, nullptr);
			stream << '\t' << "Compiler available: " << (compilerAvailable ? "true" : "false") << std::endl;

			cl_ulong globalMemSize;
			clGetDeviceInfo (*j, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, nullptr);
			stream << '\t' << "Global mem size: " << globalMemSize << std::endl;

			cl_uint computeUnitsAvailable;
			clGetDeviceInfo (*j, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(computeUnitsAvailable), &computeUnitsAvailable, nullptr);
			stream << '\t' << "Compute units available: " << computeUnitsAvailable << std::endl;
		}
	}
}

nano_pow::opencl_driver::opencl_driver (unsigned short platform_id, unsigned short device_id) :
threads (1024)
{
	auto & platform (environment.platforms[platform_id]);
	selected_device = platform.devices[device_id];
	cl_context_properties contextProperties[] = {
		CL_CONTEXT_PLATFORM,
		reinterpret_cast<cl_context_properties> (platform.platform),
		0, 0
	};
	cl_int createContextError (0);
	context = clCreateContext (contextProperties, 1, &selected_device, nullptr, nullptr, &createContextError);
	if (createContextError == CL_SUCCESS)
	{
		cl_int program_error (0);
		char const * program_data (opencl_program.data ());
		size_t program_length (opencl_program.size ());
		program = clCreateProgramWithSource (context, 1, &program_data, &program_length, &program_error);
		if (program_error == CL_SUCCESS)
		{
			auto clBuildProgramError (clBuildProgram (program, 1, &selected_device, "", nullptr, nullptr));
			if (clBuildProgramError == CL_SUCCESS)
			{
				cl_int error;
				fill = clCreateKernel (program, "fill", &error);
				search = clCreateKernel (program, "search", &error);
				result_buffer = clCreateBuffer (context, CL_MEM_WRITE_ONLY, sizeof (uint64_t), nullptr, &error);
				error = clSetKernelArg (search, 0, sizeof (result_buffer), &result_buffer);
				nonce_buffer = clCreateBuffer (context, CL_MEM_READ_WRITE, sizeof (uint64_t) * 2, nullptr, &error);
				error = clSetKernelArg (search, 3, sizeof (nonce_buffer), &nonce_buffer);
				error = clSetKernelArg (fill, 2, sizeof (nonce_buffer), &nonce_buffer);
				queue = clCreateCommandQueue (context, selected_device, 0, &error);
				error = clFinish(queue);
				error = clSetKernelArg (search, 4, sizeof (uint32_t), &stepping);
				error = clSetKernelArg (fill, 3, sizeof (uint32_t), &stepping);
			}
			else
			{
				std::cerr << "Build program error " << clBuildProgramError << std::endl;
				size_t log_size (0);
				clGetProgramBuildInfo (program, selected_device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
				std::vector<char> log (log_size);
				clGetProgramBuildInfo (program, selected_device, CL_PROGRAM_BUILD_LOG, log.size (), log.data (), nullptr);
				std::cerr << (log.data ());
			}
		}
		else
		{
			std::cerr << "Create program error " << program_error << std::endl;
		}
	}
	else
	{
		std::cerr << "Unable to create context " << createContextError << std::endl;
	}
}

nano_pow::opencl_driver::~opencl_driver ()
{
	if (slab)
	{
		clReleaseMemObject (slab);
		slab = 0;
	}
	if (fill)
	{
		clReleaseKernel (fill);
		fill = 0;
	}
	if (search)
	{
		clReleaseKernel (search);
		search = 0;
	}
	if (result_buffer)
	{
		clReleaseMemObject (result_buffer);
		result_buffer = 0;
	}
	if (nonce_buffer)
	{
		clReleaseMemObject (nonce_buffer);
		nonce_buffer = 0;
	}
	if (queue)
	{
		clReleaseCommandQueue (queue);
		queue = 0;
	}
}

void nano_pow::opencl_driver::difficulty_set (uint64_t difficulty_a)
{
	this->difficulty_inv = nano_pow::context::reverse (difficulty_a);
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

void nano_pow::opencl_driver::memory_set (size_t memory)
{
	slab_size = memory;
	slab_entries = memory / sizeof (uint32_t);
	if (slab)
	{
		clReleaseMemObject (slab);
		slab = nullptr;
	}
	cl_int error;
	slab = clCreateBuffer (context, CL_MEM_READ_WRITE, slab_size, nullptr, &error);
	assert (error == CL_SUCCESS);
}

bool nano_pow::opencl_driver::error () const
{
	return fill == nullptr || search == nullptr|| queue == nullptr || result_buffer == nullptr || slab == nullptr || nonce_buffer == nullptr;
}

namespace
{
void check (bool & error, int32_t & code, int32_t result)
{
	code = result;
	error |= code != 0;
}
}

void nano_pow::opencl_driver::fill_loop () const
{
	uint32_t current (0);
	bool error (false);
	int32_t code;
	
	uint32_t thread_count (this->threads);
	size_t threads[] = { thread_count, 0, 0 };
	
	check (error, code, clFinish (queue));
	check (error, code, clSetKernelArg (fill, 4, sizeof (uint32_t), &current));
	current += thread_count * stepping;
	check (error, code, clEnqueueNDRangeKernel (queue, fill, 1, nullptr, threads, nullptr, 0, nullptr, nullptr));
	while (!error && current < slab_entries)
	{
		check (error, code, clSetKernelArg (fill, 4, sizeof (uint32_t), &current));
		current += thread_count * stepping;
		check (error, code, clEnqueueNDRangeKernel (queue, fill, 1, nullptr, threads, nullptr, 0, nullptr, nullptr));
	}
	check (error, code, clFinish (queue));
	assert(!error);
}

uint64_t nano_pow::opencl_driver::search_loop () const
{
	std::array <cl_event, 2> events;
	bool error (false);
	int32_t code;
	uint32_t current (0);
	uint64_t result (0);
	
	uint32_t thread_count (this->threads);
	size_t threads[] = { thread_count, 0, 0 };
	
	check (error, code, clSetKernelArg (search, 5, sizeof (uint32_t), &current));
	current += thread_count * stepping;
	check (error, code, clEnqueueNDRangeKernel (queue, search, 1, nullptr, threads, nullptr, 0, nullptr, nullptr));
	check (error, code, clEnqueueReadBuffer (queue, result_buffer, false, 0, sizeof (uint64_t), &result, 0, nullptr, &events[0]));
	while (!error && result == 0)
	{
		check (error, code, clSetKernelArg (search, 5, sizeof (uint32_t), &current));
		current += thread_count * stepping;
		check (error, code, clEnqueueNDRangeKernel (queue, search, 1, nullptr, threads, nullptr, 0, nullptr, nullptr));
		check (error, code, clEnqueueReadBuffer (queue, result_buffer, false, 0, sizeof (uint64_t), &result, 0, nullptr, &events[1]));
		check (error, code, clWaitForEvents(1, &events[0]));
		events[0] = events[1];
	}
	check (error, code, clFinish (queue));
	assert (!error);
	return result;
}

uint64_t nano_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	uint64_t result (0);
	bool error (false);
	int32_t code;

	check (error, code, clSetKernelArg (search, 1, sizeof (slab), &slab));
	check (error, code, clSetKernelArg (search, 2, sizeof (uint64_t), &slab_entries));
	check (error, code, clSetKernelArg (search, 6, sizeof (uint64_t), &difficulty_inv));

	check (error, code, clSetKernelArg (fill, 0, sizeof (slab), &slab));
	check (error, code, clSetKernelArg (fill, 1, sizeof (uint64_t), &slab_entries));

	check (error, code, clEnqueueWriteBuffer (queue, result_buffer, false, 0, sizeof (result), &result, 0, nullptr, nullptr));
	check (error, code, clEnqueueWriteBuffer (queue, nonce_buffer, false, 0, sizeof (uint64_t) * 2, nonce.data (), 0, nullptr, nullptr));
	fill_loop ();
	result = search_loop ();
	return result;
}

void nano_pow::opencl_driver::dump () const
{
	nano_pow::opencl_environment environment;
	environment.dump (std::cout);
}
