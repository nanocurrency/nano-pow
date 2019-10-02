static __constant ulong lhs_or_mask = ~(ulong) (LONG_MAX);
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

typedef struct
{
	ulong low, high;
} uint128_t;
uint128_t sum (uint128_t const item_1, uint128_t const item_2)
{
	uint128_t result;
	result.low = item_1.low + item_2.low;
	if (result.low < item_1.low || result.low < item_2.low)
	{
		result.high = item_1.high + item_2.high + 1;
	}
	else
	{
		result.high = item_1.high + item_2.high;
	}
	return result;
}
bool greater (uint128_t const item_1, uint128_t const item_2)
{
	bool result = false;
	if (item_1.high > item_2.high)
	{
		result = true;
	}
	else if (item_1.high == item_2.high && item_1.low > item_2.low)
	{
		result = true;
	}
	return result;
}
uint128_t bitwise_not (uint128_t const item_a)
{
	uint128_t result;
	result.high = ~item_a.high;
	result.low = ~item_a.low;
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

#define ROTL(x, b) (uint64_t) (((x) << (b)) | ((x) >> (64 - (b))))

#define U32TO8_LE(p, v)             \
	(p)[0] = (uint8_t) ((v));       \
	(p)[1] = (uint8_t) ((v) >> 8);  \
	(p)[2] = (uint8_t) ((v) >> 16); \
	(p)[3] = (uint8_t) ((v) >> 24);

#define U64TO8_LE(p, v)                \
	U32TO8_LE ((p), (uint32_t) ((v))); \
	U32TO8_LE ((p) + 4, (uint32_t) ((v) >> 32));

#define U8TO64_LE(p) \
	(((uint64_t) ((p)[0])) | ((uint64_t) ((p)[1]) << 8) | ((uint64_t) ((p)[2]) << 16) | ((uint64_t) ((p)[3]) << 24) | ((uint64_t) ((p)[4]) << 32) | ((uint64_t) ((p)[5]) << 40) | ((uint64_t) ((p)[6]) << 48) | ((uint64_t) ((p)[7]) << 56))

#define SIPROUND            \
	do                      \
	{                       \
		v0 += v1;           \
		v1 = ROTL (v1, 13); \
		v1 ^= v0;           \
		v0 = ROTL (v0, 32); \
		v2 += v3;           \
		v3 = ROTL (v3, 16); \
		v3 ^= v2;           \
		v0 += v3;           \
		v3 = ROTL (v3, 21); \
		v3 ^= v0;           \
		v2 += v1;           \
		v1 = ROTL (v1, 17); \
		v1 ^= v2;           \
		v2 = ROTL (v2, 32); \
	} while (0)

#ifdef DEBUG
#define TRACE                                                              \
	do                                                                     \
	{                                                                      \
		printf ("(%3d) v0 %08x %08x\n", (int)inlen, (uint32_t) (v0 >> 32), \
		(uint32_t)v0);                                                     \
		printf ("(%3d) v1 %08x %08x\n", (int)inlen, (uint32_t) (v1 >> 32), \
		(uint32_t)v1);                                                     \
		printf ("(%3d) v2 %08x %08x\n", (int)inlen, (uint32_t) (v2 >> 32), \
		(uint32_t)v2);                                                     \
		printf ("(%3d) v3 %08x %08x\n", (int)inlen, (uint32_t) (v3 >> 32), \
		(uint32_t)v3);                                                     \
	} while (0)
#else
#define TRACE
#endif

int siphash (const uint8_t * in, const size_t inlen, const uint8_t * k,
uint8_t * out, const size_t outlen)
{
	uint64_t v0 = 0x736f6d6570736575UL;
	uint64_t v1 = 0x646f72616e646f6dUL;
	uint64_t v2 = 0x6c7967656e657261UL;
	uint64_t v3 = 0x7465646279746573UL;
	uint64_t k0 = U8TO64_LE (k);
	uint64_t k1 = U8TO64_LE (k + 8);
	uint64_t m;
	int i;
	const uint8_t * end = in + inlen - (inlen % sizeof (uint64_t));
	const int left = inlen & 7;
	uint64_t b = ((uint64_t)inlen) << 56;
	v3 ^= k1;
	v2 ^= k0;
	v1 ^= k1;
	v0 ^= k0;

	if (outlen == 16)
		v1 ^= 0xee;

	for (; in != end; in += 8)
	{
		m = U8TO64_LE (in);
		v3 ^= m;

		TRACE;
		for (i = 0; i < cROUNDS; ++i)
			SIPROUND;

		v0 ^= m;
	}

	switch (left)
	{
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
	U64TO8_LE (out, b);

	if (outlen == 8)
		return 0;

	v1 ^= 0xdd;

	TRACE;
	for (i = 0; i < dROUNDS; ++i)
		SIPROUND;

	b = v0 ^ v1 ^ v2 ^ v3;
	U64TO8_LE (out + 8, b);

	return 0;
}

static uint128_t hash (nonce_t const nonce_a, ulong const item_a)
{
	uint128_t result;
	int code = siphash ((uchar *)&item_a, sizeof (item_a), (uchar *)nonce_a.values, (uchar *)&result, sizeof (result));
	return result;
}

static uint128_t H0 (nonce_t nonce_a, ulong const item_a)
{
	return hash (lhs_nonce (nonce_a), item_a);
}

static uint128_t H1 (nonce_t nonce_a, ulong const item_a)
{
	return hash (rhs_nonce (nonce_a), item_a);
}

static bool passes_quick (uint128_t const sum_a, uint128_t const difficulty_inv_a)
{
	bool passed = ((sum_a.high & difficulty_inv_a.high) == 0) & ((sum_a.low & difficulty_inv_a.low) == 0);
	return passed;
}

static ulong reverse_64 (ulong const item_a)
{
	ulong result = item_a;
	result = ((result >> 1) & 0x5555555555555555) | ((result & 0x5555555555555555) << 1);
	result = ((result >> 2) & 0x3333333333333333) | ((result & 0x3333333333333333) << 2);
	result = ((result >> 4) & 0x0F0F0F0F0F0F0F0F) | ((result & 0x0F0F0F0F0F0F0F0F) << 4);
	result = ((result >> 8) & 0x00FF00FF00FF00FF) | ((result & 0x00FF00FF00FF00FF) << 8);
	result = ((result >> 16) & 0x0000FFFF0000FFFF) | ((result & 0x0000FFFF0000FFFF) << 16);
	result = (result >> 32) | (result << 32);
	return result;
}

static uint128_t reverse (uint128_t const item_a)
{
	uint128_t result;
	result.high = reverse_64 (item_a.low);
	result.low = reverse_64 (item_a.high);
	return result;
}

static bool passes_sum (uint128_t const sum_a, uint128_t const threshold_a)
{
	bool passed = greater (reverse (bitwise_not (sum_a)), threshold_a);
	return passed;
}

static uint slab (uint const slabs_a, ulong const size_a, ulong const item_a)
{
	ulong mask = size_a - 1;
	return (item_a & mask) % slabs_a;
}

static ulong bucket (uint const slabs_a, ulong const size_a, ulong const item_a)
{
	ulong mask = size_a - 1;
	return (item_a & mask) / slabs_a;
}

__kernel void search (ulong const size_a, __global ulong * const nonce_a, uint const count_a, ulong const begin_a, uint const slabs_a,
__global uint * const slab_0, __global uint * const slab_1, __global uint * const slab_2, __global uint * const slab_3,
uint128_t const threshold_a, __global ulong * result_a)
{
	//printf ("[%llu] Search (%llx%llx) size %llu begin %lu count %lu\n", get_global_id (0), threshold_a.high, threshold_a.low, size_a, begin_a, count_a);
	bool incomplete = true;
	uint lhs;
	ulong rhs;
	nonce_t nonce_l;
	nonce_l.values[0] = nonce_a[0];
	nonce_l.values[1] = nonce_a[1];
	// Local array of pointers to global memory, ~2% better performance than using a global array
	__global uint * __local slabs[4];
	slabs[0] = slab_0;
	slabs[1] = slab_1;
	slabs[2] = slab_2;
	slabs[3] = slab_3;
	for (ulong current = begin_a + get_global_id (0) * count_a, end = current + count_a; incomplete && current < end; ++current)
	{
		rhs = current;
		uint128_t const hash_l = H1 (nonce_l, rhs);
		uint const slab_l = slab (slabs_a, size_a, 0 - hash_l.low);
		ulong const bucket_l = bucket (slabs_a, size_a, 0 - hash_l.low);
		//printf("%llu %llu %lu --- %llu\n", size_a, 0 - hash_l, slab_l, (0 - hash_l) & (size_a - 1));
		lhs = slabs[slab_l][bucket_l];
		uint128_t summ = sum (H0 (nonce_l, lhs), hash_l);
		//printf ("%lu %lx %lu %lx\n", lhs, hash_l, rhs, summ);
		incomplete = !passes_quick (summ, threshold_a) || !passes_sum (summ, reverse (threshold_a));
	}
	if (!incomplete)
	{
		result_a[0] = (ulong)lhs;
		result_a[1] = rhs;
	}
}

__kernel void fill (ulong const size_a, __global ulong * const nonce_a, uint const count_a, uint const begin_a, uint const slabs_a,
__global uint * slab_0, __global uint * slab_1, __global uint * slab_2, __global uint * slab_3)
{
	//printf ("[%llu] Fill size %llu begin %lu count %lu\n", get_global_id (0), size_a, begin_a, count_a);
	nonce_t nonce_l;
	nonce_l.values[0] = nonce_a[0];
	nonce_l.values[1] = nonce_a[1];
	// Local array of pointers to global memory, ~2% better performance than using a global array
	__global uint * __local slabs[4];
	slabs[0] = slab_0;
	slabs[1] = slab_1;
	slabs[2] = slab_2;
	slabs[3] = slab_3;
	for (uint current = begin_a + get_global_id (0) * count_a, end = current + count_a; current < end; ++current)
	{
		uint128_t const hash_l = H0 (nonce_l, current);
		uint const slab_l = slab (slabs_a, size_a, hash_l.low);
		ulong const bucket_l = bucket (slabs_a, size_a, hash_l.low);
		slabs[slab_l][bucket_l] = current;
		//printf ("[%llu] Writing current %lu to slab %lu bucket %llu\n", get_global_id (0), current, slab_l, bucket_l);
	}
}
