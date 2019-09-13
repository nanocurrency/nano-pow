static __constant ulong lhs_or_mask = ~(ulong)(LONG_MAX);
static __constant ulong rhs_and_mask = LONG_MAX;
typedef struct
{
	ulong values[2];
} nonce_t;
nonce_t lhs_nonce(nonce_t item_a)
{
	nonce_t result = item_a;
	result.values[0] |= lhs_or_mask;
	return result;
}
nonce_t rhs_nonce(nonce_t item_a)
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

int siphash(const uint8_t* in, const size_t inlen, const uint8_t* k,
	uint8_t* out, const size_t outlen) {

	uint64_t v0 = 0x736f6d6570736575UL;
	uint64_t v1 = 0x646f72616e646f6dUL;
	uint64_t v2 = 0x6c7967656e657261UL;
	uint64_t v3 = 0x7465646279746573UL;
	uint64_t k0 = U8TO64_LE(k);
	uint64_t k1 = U8TO64_LE(k + 8);
	uint64_t m;
	int i;
	const uint8_t* end = in + inlen - (inlen % sizeof(uint64_t));
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

static ulong slot(ulong const size_a, ulong const item_a)
{
	ulong mask = size_a - 1;
	return item_a & mask;
}

static ulong hash(nonce_t const nonce_a, ulong const item_a)
{
	ulong result;
	int code = siphash((uchar*)& item_a, sizeof(item_a), (uchar*)nonce_a.values, (uchar*)& result, sizeof(result));
	return result;
}

static ulong H0(nonce_t nonce_a, ulong const item_a)
{
	return hash(lhs_nonce(nonce_a), item_a);
}

static ulong H1(nonce_t nonce_a, ulong const item_a)
{
	return hash(rhs_nonce(nonce_a), item_a);
}

static ulong difficulty_quick(ulong const sum_a, ulong const difficulty_inv_a)
{
	return sum_a & difficulty_inv_a;
}

static bool passes_quick(ulong const sum_a, ulong const difficulty_inv_a)
{
	bool passed = difficulty_quick(sum_a, difficulty_inv_a) == 0;
	return passed;
}

static ulong reverse(ulong const item_a)
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

static bool passes_sum(ulong const sum_a, ulong threshold_a)
{
	bool passed = reverse(~sum_a) > threshold_a;
	return passed;
}

static ulong read_value(__global uchar* const slab_a, ulong const index_a)
{
	const ulong offset_l = index_a * NP_VALUE_SIZE;
	ulong result_l = 0;
	result_l |= ((ulong) slab_a[offset_l + 0]) << 0x00;
	result_l |= ((ulong) slab_a[offset_l + 1]) << 0x08;
	result_l |= ((ulong) slab_a[offset_l + 2]) << 0x10;
	result_l |= ((ulong) slab_a[offset_l + 3]) << 0x18;
#if NP_VALUE_SIZE > 4
	result_l |= (ulong) (slab_a[offset_l + 4]) << 0x20;
#endif
#if NP_VALUE_SIZE > 5
	result_l |= (ulong) (slab_a[offset_l + 5]) << 0x28;
#endif
	return result_l;
}

__kernel void search(__global ulong* result_a, __global uint* const slab_a, ulong const size_a, __global ulong* const nonce_a, uint const count_a, uint const begin_a, ulong const threshold_a)
{
	//printf ("[%llu] Search (%llx) size %llu begin %lu count %lu\n", get_global_id (0), threshold_a, size_a, begin_a, count_a);
	bool incomplete = true;
	uint lhs, rhs;
	nonce_t nonce_l;
	nonce_l.values[0] = nonce_a[0];
	nonce_l.values[1] = nonce_a[1];
	for (uint current = begin_a + get_global_id(0) * count_a, end = current + count_a; incomplete && current < end; ++current)
	{
		rhs = current;
		ulong hash_l = H1(nonce_l, rhs);
		lhs = read_value(slab_a, slot(size_a, 0 - hash_l));
		//lhs = slab_a[slot(size_a, 0 - hash_l)];
		ulong sum = H0(nonce_l, lhs) + hash_l;
		//printf ("%lu %lx %lu %lx\n", lhs, hash_l, rhs, sum);
		incomplete = !passes_quick(sum, threshold_a) || !passes_sum(sum, reverse(threshold_a));
	}
	if (!incomplete)
	{
		*result_a = (((ulong)(lhs)) << 32) | rhs;
	}
}

static void write_value(__global uchar* const slab_a, ulong const index_a, uint value_a)
{
	const ulong offset_l = index_a * NP_VALUE_SIZE;
	slab_a[offset_l + 0] = (uchar) (value_a >> 0x00);
	slab_a[offset_l + 1] = (uchar) (value_a >> 0x08);
	slab_a[offset_l + 2] = (uchar) (value_a >> 0x10);
	slab_a[offset_l + 3] = (uchar) (value_a >> 0x18);
#if NP_VALUE_SIZE > 4
	slab_a[offset_l + 4] = (uchar) (value_a >> 0x20);
#endif
#if NP_VALUE_SIZE > 5
	slab_a[offset_l + 5] = (uchar) (value_a >> 0x28);
#endif
}

__kernel void fill(__global uint* const slab_a, ulong const size_a, __global ulong* const nonce_a, uint const count_a, uint const begin_a)
{
	//printf ("[%llu] Fill size %llu begin %lu count %lu\n", get_global_id (0), size_a, begin_a, count_a);
	nonce_t nonce_l;
	nonce_l.values[0] = nonce_a[0];
	nonce_l.values[1] = nonce_a[1];
	for (uint current = begin_a + get_global_id(0) * count_a, end = current + count_a; current < end; ++current)
	{
		ulong slot_l = slot(size_a, H0(nonce_l, current));
		write_value(slab_a, slot_l, current);
		//slab_a[slot_l] = current;
		//printf ("[%llu] Writing current %lu to slot %llu\n", get_global_id (0), current, slot_l);
	}
}
