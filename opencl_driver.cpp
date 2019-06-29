#include <opencl_driver.hpp>

#include <ssp_pow/hash.hpp>
#include <ssp_pow/pow.hpp>

#include <boost/endian/arithmetic.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <chrono>

std::string opencl_program = R"%%%(
enum blake2b_constant
{
	BLAKE2B_BLOCKBYTES = 128,
	BLAKE2B_OUTBYTES   = 64,
	BLAKE2B_KEYBYTES   = 64,
	BLAKE2B_SALTBYTES  = 16,
	BLAKE2B_PERSONALBYTES = 16
};

typedef struct __blake2b_param
{
	uchar  digest_length; // 1
	uchar  key_length;    // 2
	uchar  fanout;        // 3
	uchar  depth;         // 4
	uint leaf_length;   // 8
	ulong node_offset;   // 16
	uchar  node_depth;    // 17
	uchar  inner_length;  // 18
	uchar  reserved[14];  // 32
	uchar  salt[BLAKE2B_SALTBYTES]; // 48
	uchar  personal[BLAKE2B_PERSONALBYTES];  // 64
} blake2b_param;

typedef struct __blake2b_state
{
	ulong h[8];
	ulong t[2];
	ulong f[2];
	uchar  buf[2 * BLAKE2B_BLOCKBYTES];
	size_t   buflen;
	uchar  last_node;
} blake2b_state;

__constant static const ulong blake2b_IV[8] =
{
	0x6a09e667f3bcc908UL, 0xbb67ae8584caa73bUL,
	0x3c6ef372fe94f82bUL, 0xa54ff53a5f1d36f1UL,
	0x510e527fade682d1UL, 0x9b05688c2b3e6c1fUL,
	0x1f83d9abfb41bd6bUL, 0x5be0cd19137e2179UL
};

__constant static const uchar blake2b_sigma[12][16] =
{
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};


static inline int blake2b_set_lastnode( blake2b_state *S )
{
	S->f[1] = ~0UL;
	return 0;
}

/* Some helper functions, not necessarily useful */
static inline int blake2b_set_lastblock( blake2b_state *S )
{
	if( S->last_node ) blake2b_set_lastnode( S );
	
	S->f[0] = ~0UL;
	return 0;
}

static inline int blake2b_increment_counter( blake2b_state *S, const ulong inc )
{
	S->t[0] += inc;
	S->t[1] += ( S->t[0] < inc );
	return 0;
}

static inline ulong load64( const void *src )
{
#if defined(__ENDIAN_LITTLE__)
	return *( ulong * )( src );
#else
	const uchar *p = ( uchar * )src;
	ulong w = *p++;
	w |= ( ulong )( *p++ ) <<  8;
	w |= ( ulong )( *p++ ) << 16;
	w |= ( ulong )( *p++ ) << 24;
	w |= ( ulong )( *p++ ) << 32;
	w |= ( ulong )( *p++ ) << 40;
	w |= ( ulong )( *p++ ) << 48;
	w |= ( ulong )( *p++ ) << 56;
	return w;
#endif
}

static inline void store32( void *dst, uint w )
{
#if defined(__ENDIAN_LITTLE__)
	*( uint * )( dst ) = w;
#else
	uchar *p = ( uchar * )dst;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w;
#endif
}

static inline void store64( void *dst, ulong w )
{
#if defined(__ENDIAN_LITTLE__)
	*( ulong * )( dst ) = w;
#else
	uchar *p = ( uchar * )dst;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w; w >>= 8;
	*p++ = ( uchar )w;
#endif
}

static inline ulong rotr64( const ulong w, const unsigned c )
{
	return ( w >> c ) | ( w << ( 64 - c ) );
}

static void ucharset (void * dest_a, int val, size_t count)
{
	uchar * dest = (uchar *)dest_a;
	for (size_t i = 0; i < count; ++i)
	{
		*dest++ = val;
	}
}

/* init xors IV with input parameter block */
static inline int blake2b_init_param( blake2b_state *S, const blake2b_param *P )
{
	uchar *p, *h;
	__constant uchar *v;
	v = ( __constant uchar * )( blake2b_IV );
	h = ( uchar * )( S->h );
	p = ( uchar * )( P );
	/* IV XOR ParamBlock */
	ucharset( S, 0, sizeof( blake2b_state ) );
	
	for( int i = 0; i < BLAKE2B_OUTBYTES; ++i ) h[i] = v[i] ^ p[i];
	
	return 0;
}

static inline int blake2b_init( blake2b_state *S, const uchar outlen )
{
	blake2b_param P[1];
	
	if ( ( !outlen ) || ( outlen > BLAKE2B_OUTBYTES ) ) return -1;
	
	P->digest_length = outlen;
	P->key_length    = 0;
	P->fanout        = 1;
	P->depth         = 1;
	store32( &P->leaf_length, 0 );
	store64( &P->node_offset, 0 );
	P->node_depth    = 0;
	P->inner_length  = 0;
	ucharset( P->reserved, 0, sizeof( P->reserved ) );
	ucharset( P->salt,     0, sizeof( P->salt ) );
	ucharset( P->personal, 0, sizeof( P->personal ) );
	return blake2b_init_param( S, P );
}

static int blake2b_compress( blake2b_state *S, __private const uchar block[BLAKE2B_BLOCKBYTES] )
{
	ulong m[16];
	ulong v[16];
	int i;
	
	for( i = 0; i < 16; ++i )
		m[i] = load64( block + i * sizeof( m[i] ) );
	
	for( i = 0; i < 8; ++i )
		v[i] = S->h[i];
	
	v[ 8] = blake2b_IV[0];
	v[ 9] = blake2b_IV[1];
	v[10] = blake2b_IV[2];
	v[11] = blake2b_IV[3];
	v[12] = S->t[0] ^ blake2b_IV[4];
	v[13] = S->t[1] ^ blake2b_IV[5];
	v[14] = S->f[0] ^ blake2b_IV[6];
	v[15] = S->f[1] ^ blake2b_IV[7];
#define G(r,i,a,b,c,d) \
do { \
a = a + b + m[blake2b_sigma[r][2*i+0]]; \
d = rotr64(d ^ a, 32); \
c = c + d; \
b = rotr64(b ^ c, 24); \
a = a + b + m[blake2b_sigma[r][2*i+1]]; \
d = rotr64(d ^ a, 16); \
c = c + d; \
b = rotr64(b ^ c, 63); \
} while(0)
#define ROUND(r)  \
do { \
G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
G(r,2,v[ 2],v[ 6],v[10],v[14]); \
G(r,3,v[ 3],v[ 7],v[11],v[15]); \
G(r,4,v[ 0],v[ 5],v[10],v[15]); \
G(r,5,v[ 1],v[ 6],v[11],v[12]); \
G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
} while(0)
	ROUND( 0 );
	ROUND( 1 );
	ROUND( 2 );
	ROUND( 3 );
	ROUND( 4 );
	ROUND( 5 );
	ROUND( 6 );
	ROUND( 7 );
	ROUND( 8 );
	ROUND( 9 );
	ROUND( 10 );
	ROUND( 11 );
	
	for( i = 0; i < 8; ++i )
		S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
	
#undef G
#undef ROUND
	return 0;
}

static void ucharcpy (uchar * dst, uchar const * src, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		*dst++ = *src++;
	}
}

static void printstate (blake2b_state * S)
{
	printf ("%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu ", S->h[0], S->h[1], S->h[2], S->h[3], S->h[4], S->h[5], S->h[6], S->h[7], S->t[0], S->t[1], S->f[0], S->f[1]);
	for (int i = 0; i < 256; ++i)
	{
		printf ("%02x", S->buf[i]);
	}
	printf (" %lu %02x\n", S->buflen, S->last_node);
}

/* inlen now in bytes */
static int blake2b_update( blake2b_state *S, const uchar *in, ulong inlen )
{
	while( inlen > 0 )
	{
		size_t left = S->buflen;
		size_t fill = 2 * BLAKE2B_BLOCKBYTES - left;
		
		if( inlen > fill )
		{
			ucharcpy( S->buf + left, in, fill ); // Fill buffer
			S->buflen += fill;
			blake2b_increment_counter( S, BLAKE2B_BLOCKBYTES );
			blake2b_compress( S, S->buf ); // Compress
			ucharcpy( S->buf, S->buf + BLAKE2B_BLOCKBYTES, BLAKE2B_BLOCKBYTES ); // Shift buffer left
			S->buflen -= BLAKE2B_BLOCKBYTES;
			in += fill;
			inlen -= fill;
		}
		else // inlen <= fill
		{
			ucharcpy( S->buf + left, in, inlen );
			S->buflen += inlen; // Be lazy, do not compress
			in += inlen;
			inlen -= inlen;
		}
	}
	
	return 0;
}

/* Is this correct? */
static int blake2b_final( blake2b_state *S, uchar *out, uchar outlen )
{
	uchar buffer[BLAKE2B_OUTBYTES];
	
	if( S->buflen > BLAKE2B_BLOCKBYTES )
	{
		blake2b_increment_counter( S, BLAKE2B_BLOCKBYTES );
		blake2b_compress( S, S->buf );
		S->buflen -= BLAKE2B_BLOCKBYTES;
		ucharcpy( S->buf, S->buf + BLAKE2B_BLOCKBYTES, S->buflen );
	}
	
	//blake2b_increment_counter( S, S->buflen );
	ulong inc = (ulong)S->buflen;
	S->t[0] += inc;
	//  if ( S->t[0] < inc )
	//    S->t[1] += 1;
	// This seems to crash the opencl compiler though fortunately this is calculating size and we don't do things bigger than 2^32
	
	blake2b_set_lastblock( S );
	ucharset( S->buf + S->buflen, 0, 2 * BLAKE2B_BLOCKBYTES - S->buflen ); /* Padding */
	blake2b_compress( S, S->buf );
	
	for( int i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
		store64( buffer + sizeof( S->h[i] ) * i, S->h[i] );
	
	ucharcpy( out, buffer, outlen );
	return 0;
}

static void ucharcpyglb (uchar * dst, __global uchar const * src, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		*dst = *src;
		++dst;
		++src;
	}
}

static ulong slot (ulong const size_a, ulong const item_a)
{
	ulong mask = size_a - 1;
	return item_a & mask;
}

static ulong reduce (ulong const item_a, ulong threshold_a)
{
	//printf ("Reduction: %llx\n", item_a & threshold_a);
	return item_a & threshold_a;
}

static ulong hash (blake2b_state const * state_a, ulong const item_a)
{
	ulong result;
	blake2b_state state_l = *state_a;
	blake2b_update (&state_l, (uchar *)&item_a, sizeof (item_a));
	blake2b_final (&state_l, (uchar *)&result, sizeof (result));
	return result;
}

static void init (ulong * const nonce_a, blake2b_state * state_a)
{
	blake2b_init (state_a, sizeof (ulong));
	blake2b_update (state_a, (uchar const *)nonce_a, sizeof (ulong) * 2);
}

__kernel void search (__global ulong * result_a, __global uint * const slab_a, ulong const size_a, __global ulong * const nonce_a, uint const count, uint const begin, ulong const threshold_a)
{
	//printf ("Slab %llu %llu %llx\n", slab_a, size_a, threshold_a);
	ulong nonce_l[2] = { nonce_a [0], nonce_a [1] };
	blake2b_state state;
	init (nonce_l, &state);
	bool incomplete = true;
	uint lhs, rhs;
	for (uint current = begin + get_global_id (0) * count, end = current + count; incomplete && current < end; ++current)
	{
		lhs = current;
		ulong hash_l = hash (&state, current | (0x1UL << 63));
		rhs = slab_a [slot (size_a, 0 - hash_l)];
		ulong hash2 = hash (&state, rhs & 0x7fffffff);
		//printf ("%lu %lx %lu %lx\n", lhs, hash_l, rhs, hash2);
		incomplete = reduce (hash_l + hash2, threshold_a) != 0;
	}
	if (!incomplete)
	{
		*result_a = (((ulong) (lhs)) << 32) | rhs;
	}
}

__kernel void fill (__global uint * const slab_a, ulong const size_a, __global ulong * const nonce_a, uint const count, uint const begin)
{
	//printf ("Slab %llu %llu\n", slab_a, size_a);
	ulong nonce_l[2] = { nonce_a [0], nonce_a [1] };
	blake2b_state state;
	init (nonce_l, &state);
	for (uint current = begin + get_global_id (0) * count, end = current + count; current < end && current < size_a; ++current)
	{
		//printf ("Writing %llu to %llu\n", current, slot (size_a, hash (&state, current & 0x7fffffff)));
		slab_a [slot (size_a, hash (&state, current & 0x7fffffff))] = current;
	}
}
)%%%";

ssp_pow::opencl_environment::opencl_environment ()
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

void ssp_pow::opencl_environment::dump (std::ostream & stream)
{
	auto index (0);
	size_t device_count (0);
	for (auto & i : platforms)
	{
		device_count += i.devices.size ();
	}
	stream << boost::str (boost::format ("OpenCL found %1% platforms and %2% devices\n") % platforms.size () % device_count);
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
			size_t deviceTypeCount = 0;
			clGetDeviceInfo (*j, CL_DEVICE_TYPE, 0, nullptr, &deviceTypeCount);
			std::vector<uint8_t> deviceTypeInfo (deviceTypeCount);
			clGetDeviceInfo (*j, CL_DEVICE_TYPE, deviceTypeCount, deviceTypeInfo.data (), 0);
			std::string device_type_string;
			switch (deviceTypeInfo[0])
			{
				case CL_DEVICE_TYPE_ACCELERATOR:
					device_type_string = "ACCELERATOR";
					break;
				case CL_DEVICE_TYPE_CPU:
					device_type_string = "CPU";
					break;
				case CL_DEVICE_TYPE_CUSTOM:
					device_type_string = "CUSTOM";
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
			stream << '\t' << device_type_string << std::endl;
			
			size_t compilerAvailableCount = 0;
			clGetDeviceInfo (*j, CL_DEVICE_COMPILER_AVAILABLE, 0, nullptr, &compilerAvailableCount);
			std::vector<uint8_t> compilerAvailableInfo (compilerAvailableCount);
			clGetDeviceInfo (*j, CL_DEVICE_COMPILER_AVAILABLE, compilerAvailableCount, compilerAvailableInfo.data (), 0);
			stream << '\t' << "Compiler available: " << (compilerAvailableInfo[0] ? "true" : "false") << std::endl;
			
			size_t globalMemSizeCount = 0;
			clGetDeviceInfo (*j, CL_DEVICE_GLOBAL_MEM_SIZE, 0, nullptr, &globalMemSizeCount);
			std::vector<uint8_t> globalMemSizeInfo (globalMemSizeCount);
			clGetDeviceInfo (*j, CL_DEVICE_GLOBAL_MEM_SIZE, globalMemSizeCount, globalMemSizeInfo.data (), 0);
			uint64_t globalMemSize (boost::endian::little_to_native (*reinterpret_cast <uint64_t *> (globalMemSizeInfo.data ())));
			stream << '\t' << "Global mem size: " << globalMemSize << std::endl;
			
			size_t computeUnitsAvailableCount = 0;
			clGetDeviceInfo (*j, CL_DEVICE_MAX_COMPUTE_UNITS, 0, nullptr, &computeUnitsAvailableCount);
			std::vector<uint8_t> computeUnitsAvailableInfo (computeUnitsAvailableCount);
			clGetDeviceInfo (*j, CL_DEVICE_MAX_COMPUTE_UNITS, computeUnitsAvailableCount, computeUnitsAvailableInfo.data (), 0);
			uint32_t computeUnits (boost::endian::little_to_native (*reinterpret_cast <uint32_t *> (computeUnitsAvailableInfo.data ())));
			stream << '\t' << "Compute units available: " << computeUnits << std::endl;
		}
	}
}

ssp_pow::opencl_driver::opencl_driver (unsigned short platform_id, unsigned short device_id) :
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
				error = clSetKernelArg (search, 4, sizeof (uint32_t), &stepping);
				error = clSetKernelArg (fill, 3, sizeof (uint32_t), &stepping);
			}
			else
			{
				std::cerr << (boost::str (boost::format ("Build program error %1%\n") % clBuildProgramError));
				size_t log_size (0);
				clGetProgramBuildInfo (program, selected_device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
				std::vector<char> log (log_size);
				clGetProgramBuildInfo (program, selected_device, CL_PROGRAM_BUILD_LOG, log.size (), log.data (), nullptr);
				std::cerr << (log.data ());
			}
		}
		else
		{
			std::cerr << (boost::str (boost::format ("Create program error %1%\n") % program_error));
		}
	}
	else
	{
		std::cerr << (boost::str (boost::format ("Unable to create context %1%\n") % createContextError));
	}
}

ssp_pow::opencl_driver::~opencl_driver ()
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

void ssp_pow::opencl_driver::threshold_set (uint64_t threshold)
{
	this->threshold = threshold;
}

void ssp_pow::opencl_driver::threads_set (unsigned threads)
{
	this->threads = threads;
}

unsigned ssp_pow::opencl_driver::threads_get () const
{
	return threads;
}

void ssp_pow::opencl_driver::lookup_set (size_t lookup)
{
	this->lookup = lookup;
	if (slab)
	{
		clReleaseMemObject (slab);
		slab = nullptr;
	}
	cl_int error;
	slab = clCreateBuffer (context, CL_MEM_READ_WRITE, lookup * sizeof (uint32_t), nullptr, &error);
	assert (error == CL_SUCCESS);
}

bool ssp_pow::opencl_driver::error () const
{
	return fill == nullptr || search == nullptr|| queue == nullptr || result_buffer == nullptr || slab == nullptr || nonce_buffer == nullptr;
}

uint64_t ssp_pow::opencl_driver::solve (std::array<uint64_t, 2> nonce)
{
	uint64_t result (0);
	bool error (false);
	int32_t code;
	
	error |= code = clSetKernelArg (search, 1, sizeof (slab), &slab);
	error |= code = clSetKernelArg (search, 2, sizeof (uint64_t), &lookup);
	error |= code = clSetKernelArg (search, 6, sizeof (uint64_t), &threshold);
	
	error |= code = clSetKernelArg (fill, 0, sizeof (slab), &slab);
	error |= code = clSetKernelArg (fill, 1, sizeof (uint64_t), &lookup);
	
	error |= code = clEnqueueWriteBuffer (queue, result_buffer, false, 0, sizeof (result), &result, 0, nullptr, nullptr);
	error |= code = clEnqueueWriteBuffer (queue, nonce_buffer, false, 0, sizeof (uint64_t) * 2, nonce.data (), 0, nullptr, nullptr);
	uint32_t current (0);
	error |= code = clSetKernelArg (fill, 4, sizeof (uint32_t), &current);
	size_t fill_size[] = { std::max<size_t> (1U, lookup / stepping), 0, 0 };
	error |= code = clEnqueueNDRangeKernel (queue, fill, 1, nullptr, fill_size, nullptr, 0, nullptr, nullptr);
	error |= code = clFinish (queue);
	size_t search_size[] = { threads, 0, 0 };
	std::array <cl_event, 2> events;
	error |= code = clSetKernelArg (search, 5, sizeof (uint32_t), &current);
	current += threads * stepping;
	error |= code = clEnqueueNDRangeKernel (queue, search, 1, nullptr, search_size, nullptr, 0, nullptr, nullptr);
	error |= code = clEnqueueReadBuffer (queue, result_buffer, false, 0, sizeof (uint64_t), &result, 0, nullptr, &events[0]);
	while (!error && result == 0)
	{
		error |= code = clSetKernelArg (search, 5, sizeof (uint32_t), &current);
		current += threads * stepping;
		error |= code = clEnqueueNDRangeKernel (queue, search, 1, nullptr, search_size, nullptr, 0, nullptr, nullptr);
		error |= code = clEnqueueReadBuffer (queue, result_buffer, false, 0, sizeof (uint64_t), &result, 0, nullptr, &events[1]);
		error |= clWaitForEvents(1, &events[0]);
		events[0] = events[1];
	}
	error |= code = clFinish (queue);
	assert (!error);
	return result;
}
