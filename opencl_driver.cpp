#include <opencl_driver.hpp>

#include <ssp_pow/hash.hpp>
#include <ssp_pow/pow.hpp>

#include <boost/format.hpp>

#include <array>
#include <string>

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

static ulong search (__global uint * const slab_a, ulong const size_a, blake2b_state const * state_a, uint const count, uint const begin, ulong const threshold_a)
{
	bool incomplete = true;
	uint lhs, rhs;
	for (uint current = begin, end = current + count; incomplete && current < end; ++current)
	{
		lhs = current;
		ulong hash_l = hash (state_a, current | (0x1UL << 63));
		rhs = slab_a [slot (size_a, 0 - hash_l)];
		incomplete = reduce (hash_l + hash (state_a, rhs & 0x7fffffff), threshold_a) != 0;
	}
	return incomplete ? 0 : (((ulong) (lhs)) << 32) | rhs;
}

static void fill (__global uint * const slab_a, ulong const size_a, blake2b_state const * state_a, uint const count, uint const begin)
{
	for (uint current = begin, end = current + count; current < end; ++current)
	{
		slab_a [slot (size_a, hash (state_a, current & 0x7fffffff))] = current;
	}
}

__kernel void find (__global ulong * result_a, __global uint * const slab_a, ulong const size_a, __global ulong * const nonce_a, __global uint * const current, __global uint * const ticket, uint const total_threads, uint const stepping_a, ulong const threshold_a)
{
	printf ("Entering\n");
	ulong nonce_l[2] = { nonce_a[0], nonce_a[1] };
	blake2b_state state;
	blake2b_init (&state, sizeof (ulong));
	blake2b_update (&state, (uchar const *)nonce_l, sizeof (ulong) * 2);
	uint const thread = get_global_id (0);
	uint ticket_l = *ticket;
	barrier (CLK_GLOBAL_MEM_FENCE);
	uint last_fill = ~0;
	while (*ticket == ticket_l)
	{
		ulong current_l = atomic_add (current, stepping_a);
		if (current_l >> 32 != last_fill)
		{
			last_fill = current_l >> 32;
			fill (slab_a, size_a, &state, size_a / total_threads, last_fill * size_a + thread * (size_a / total_threads));
		}
		ulong result_l = search (slab_a, size_a, &state, stepping_a, current_l, threshold_a);
		if (result_l != 0)
		{
			if (atomic_add (ticket, 1) == ticket_l)
			{
				printf ("%lu\n", result_l);
				*result_a = result_l;
			}
		}
	}
}
)%%%";

opencl_pow_driver::opencl_environment::opencl_environment (bool & error_a)
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

void opencl_pow_driver::opencl_environment::dump (std::ostream & stream)
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
			size_t computeUnitsAvailableCount = 0;
			clGetDeviceInfo (*j, CL_DEVICE_MAX_COMPUTE_UNITS, 0, nullptr, &computeUnitsAvailableCount);
			std::vector<uint8_t> computeUnitsAvailableInfo (computeUnitsAvailableCount);
			clGetDeviceInfo (*j, CL_DEVICE_MAX_COMPUTE_UNITS, computeUnitsAvailableCount, computeUnitsAvailableInfo.data (), 0);
			uint64_t computeUnits (computeUnitsAvailableInfo[0] | (computeUnitsAvailableInfo[1] << 8) | (computeUnitsAvailableInfo[2] << 16) | (computeUnitsAvailableInfo[3] << 24));
			stream << '\t' << "Compute units available: " << computeUnits << std::endl;
		}
	}
}

class kernel
{
public:
	kernel (uint64_t threshold_a, cl_device_id device_a, cl_context context_a, cl_program program_a) :
	threshold (threshold_a)
	{
		cl_int error;
		kernel_m = clCreateKernel (program_a, "find", &error);
		result_buffer = clCreateBuffer (context_a, CL_MEM_WRITE_ONLY, sizeof (uint64_t), nullptr, &error); // 0
		slab_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint64_t), nullptr, &error); // 1
		size_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint64_t), nullptr, &error); // 2
		nonce_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint64_t), nullptr, &error); // 3
		current_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint32_t), nullptr, &error); // 4
		ticket_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint32_t), nullptr, &error); // 5
		total_threads_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint32_t), nullptr, &error); // 6
		stepping_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint32_t), nullptr, &error); // 7
		threshold_buffer = clCreateBuffer (context_a, CL_MEM_READ_ONLY, sizeof (uint64_t), nullptr, &error); // 8
		queue = clCreateCommandQueue (context_a, device_a, 0, &error);
	}
	void operator () (uint32_t total_threads)
	{
		bool error (false);
		int32_t code;
		size_t work_size[] = { total_threads, 0, 0 };
		error |= code = clSetKernelArg (kernel_m, 0, sizeof (result_buffer), &result_buffer);
		error |= code = clSetKernelArg (kernel_m, 1, sizeof (slab_buffer), &slab_buffer);
		error |= code = clSetKernelArg (kernel_m, 2, sizeof (size_buffer), &size_buffer);
		error |= code = clSetKernelArg (kernel_m, 3, sizeof (nonce_buffer), &nonce_buffer);
		error |= code = clSetKernelArg (kernel_m, 4, sizeof (current_buffer), &current_buffer);
		error |= code = clSetKernelArg (kernel_m, 5, sizeof (ticket_buffer), &ticket_buffer);
		error |= code = clSetKernelArg (kernel_m, 6, sizeof (uint32_t), &total_threads_buffer);
		error |= code = clSetKernelArg (kernel_m, 7, sizeof (uint32_t), &stepping_buffer);
		error |= code = clSetKernelArg (kernel_m, 8, sizeof (threshold_buffer), &threshold_buffer);

		error |= code = clEnqueueWriteBuffer (queue, slab_buffer, false, 0, sizeof (uint64_t), &slab, 0, nullptr, nullptr); // 1
		error |= code = clEnqueueWriteBuffer (queue, size_buffer, false, 0, sizeof (uint64_t), &size, 0, nullptr, nullptr); // 2
		error |= code = clEnqueueWriteBuffer (queue, nonce_buffer, false, 0, sizeof (uint64_t), &nonce, 0, nullptr, nullptr); // 3
		error |= code = clEnqueueWriteBuffer (queue, current_buffer, false, 0, sizeof (uint32_t), &current, 0, nullptr, nullptr); // 4
		error |= code = clEnqueueWriteBuffer (queue, ticket_buffer, false, 0, sizeof (uint32_t), &ticket, 0, nullptr, nullptr); // 5
		error |= code = clEnqueueWriteBuffer (queue, total_threads_buffer, false, 0, sizeof (uint32_t), &total_threads, 0, nullptr, nullptr); // 6
		error |= code = clEnqueueWriteBuffer (queue, stepping_buffer, false, 0, sizeof (uint32_t), &stepping, 0, nullptr, nullptr); // 7
		error |= code = clEnqueueWriteBuffer (queue, threshold_buffer, false, 0, sizeof (uint64_t), &threshold, 0, nullptr, nullptr); // 8

		error |= code = clEnqueueNDRangeKernel (queue, kernel_m, 1, nullptr, work_size, nullptr, 0, nullptr, nullptr);
		error |= code = clEnqueueReadBuffer (queue, result_buffer, false, 0, sizeof (uint64_t), &result, 0, nullptr, nullptr);
		error |= code = clFinish (queue);
		assert (!error);
	}
	bool error () const
	{
		return kernel_m == nullptr || result_buffer == nullptr || slab_buffer == nullptr || current_buffer == nullptr || ticket_buffer == nullptr;
	}
	uint64_t result; // 0
	cl_command_queue queue;
	cl_kernel kernel_m;
	cl_mem result_buffer; // 0
	cl_mem slab_buffer; // 1
	cl_mem size_buffer; // 2
	cl_mem nonce_buffer; // 3
	cl_mem current_buffer; // 4
	cl_mem ticket_buffer; // 5
	cl_mem total_threads_buffer; // 6
	cl_mem stepping_buffer; // 7
	cl_mem threshold_buffer; // 8
	uint64_t slab; // 1
	uint64_t size; // 2
	uint64_t nonce; // 3
	uint32_t current; // 4
	uint32_t ticket; // 5
	uint32_t stepping { 65535 }; // 7
	uint64_t threshold; // 8
};

int opencl_pow_driver::main(int argc, char **argv, unsigned short platform_id, unsigned short device_id)
{
	bool error_a (false);
	opencl_environment environment (error_a);
	cl_context context;
	cl_program program;
	auto & platform (environment.platforms[platform_id]);
	std::array<cl_device_id, 1> selected_devices;
	selected_devices[0] = platform.devices[device_id];
	cl_context_properties contextProperties[] = {
		CL_CONTEXT_PLATFORM,
		reinterpret_cast<cl_context_properties> (platform.platform),
		0, 0
	};
	cl_int createContextError (0);
	context = clCreateContext (contextProperties, static_cast<cl_uint> (selected_devices.size ()), selected_devices.data (), nullptr, nullptr, &createContextError);
	error_a |= createContextError != CL_SUCCESS;
	if (!error_a)
	{
		cl_int program_error (0);
		char const * program_data (opencl_program.data ());
		size_t program_length (opencl_program.size ());
		program = clCreateProgramWithSource (context, 1, &program_data, &program_length, &program_error);
		error_a |= program_error != CL_SUCCESS;
		if (!error_a)
		{
			auto clBuildProgramError (clBuildProgram (program, static_cast<cl_uint> (selected_devices.size ()), selected_devices.data (), "", nullptr, nullptr));
			error_a |= clBuildProgramError != CL_SUCCESS;
			if (!error_a)
			{
				std::array <uint64_t, 2> nonce = { 0, 0 };
				ssp_pow::blake2_hash hash (nonce);
				ssp_pow::context<ssp_pow::blake2_hash> ctx (hash, 32);
				kernel kernel (ctx.threshold, selected_devices[0], context, program);
				if (!kernel.error ())
				{
					kernel (1);
				}
			}
			else
			{
				std::cerr << (boost::str (boost::format ("Build program error %1%") % clBuildProgramError));
				for (auto i (selected_devices.begin ()), n (selected_devices.end ()); i != n; ++i)
				{
					size_t log_size (0);
					clGetProgramBuildInfo (program, *i, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
					std::vector<char> log (log_size);
					clGetProgramBuildInfo (program, *i, CL_PROGRAM_BUILD_LOG, log.size (), log.data (), nullptr);
					std::cerr << (log.data ());
				}
			}
		}
		else
		{
			std::cerr << (boost::str (boost::format ("Create program error %1%") % program_error));
		}
	}
	else
	{
		std::cerr << (boost::str (boost::format ("Unable to create context %1%") % createContextError));
	}
	return 0;
}
