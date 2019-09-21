#include <nano_pow/cpp_driver.hpp>

#include <nano_pow/plat.hpp>
#include <nano_pow/pow.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

/*
 SipHash reference C implementation
 
 Copyright (c) 2012-2016 Jean-Philippe Aumasson
 <jeanphilippe.aumasson@gmail.com>
 Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
 
 To the extent possible under law, the author(s) have dedicated all copyright
 and related and neighboring rights to this software to the public domain
 worldwide. This software is distributed without any warranty.
 
 You should have received a copy of the CC0 Public Domain Dedication along
 with this software. If not, see
 <http://creativecommons.org/publicdomain/zero/1.0/>.
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

NP_INLINE int siphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
						  uint8_t *out, const size_t outlen) {
	assert((outlen == 8) || (outlen == 16));
	uint64_t v0 = 0x736f6d6570736575ULL;
	uint64_t v1 = 0x646f72616e646f6dULL;
	uint64_t v2 = 0x6c7967656e657261ULL;
	uint64_t v3 = 0x7465646279746573ULL;
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

NP_INLINE static std::array<uint64_t, 2> lhs_nonce (std::array<uint64_t, 2> item_a)
{
	uint64_t lhs_or_mask (~static_cast <uint64_t> (std::numeric_limits<int64_t>::max ()));
	std::array<uint64_t, 2> result = item_a;
	result[0] |= lhs_or_mask;
	return result;
}

NP_INLINE static std::array<uint64_t, 2> rhs_nonce (std::array<uint64_t, 2> item_a)
{
	std::array<uint64_t, 2> result = item_a;
	result[0] &= std::numeric_limits<int64_t>::max ();
	return result;
}

NP_INLINE static nano_pow::uint128_t hash (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
{
	nano_pow::uint128_t result;
	auto error (siphash (reinterpret_cast<uint8_t const *> (&item_a), sizeof (item_a), reinterpret_cast<uint8_t const *> (nonce_a.data ()), reinterpret_cast<uint8_t *> (&result), sizeof (result)));
	assert (!error);
	return result;
}

// Hash function H0 sets the high order bit
NP_INLINE static nano_pow::uint128_t H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
{
	return hash (lhs_nonce (nonce_a), item_a);
}

// Hash function H0 sets the high order bit
nano_pow::uint128_t nano_pow::H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
{
	return ::H0 (nonce_a, item_a);
}

// Hash function H1 clears the high order bit
NP_INLINE static nano_pow::uint128_t H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
{
	return hash (rhs_nonce (nonce_a), item_a);
}

// Hash function H1 clears the high order bit
nano_pow::uint128_t nano_pow::H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
{
	return ::H1 (nonce_a, item_a);
}

NP_INLINE static uint64_t reverse_64 (uint64_t const item_a)
{
	auto result (item_a);
	result = ((result >>  1) & 0x5555555555555555) | ((result & 0x5555555555555555) <<  1);
	result = ((result >>  2) & 0x3333333333333333) | ((result & 0x3333333333333333) <<  2);
	result = ((result >>  4) & 0x0F0F0F0F0F0F0F0F) | ((result & 0x0F0F0F0F0F0F0F0F) <<  4);
	result = ((result >>  8) & 0x00FF00FF00FF00FF) | ((result & 0x00FF00FF00FF00FF) <<  8);
	result = ((result >> 16) & 0x0000FFFF0000FFFF) | ((result & 0x0000FFFF0000FFFF) << 16);
	result = ( result >> 32                      ) | ( result                       << 32);
	return result;
}

NP_INLINE static nano_pow::uint128_t reverse (nano_pow::uint128_t const item_a)
{
	nano_pow::uint128_t result = (static_cast<nano_pow::uint128_t> (::reverse_64 (static_cast<uint64_t> (item_a))) << 64) | ::reverse_64 (static_cast<uint64_t> (item_a >> 64));
	return result;
}

nano_pow::uint128_t nano_pow::reverse (nano_pow::uint128_t const item_a)
{
	return ::reverse (item_a);
}

nano_pow::uint128_t nano_pow::bit_difficulty (unsigned bits_a)
{
	assert (bits_a > 0 && bits_a < 128 && "Difficulty must be greater than 0 and less than 128");
	return ::reverse ((static_cast<nano_pow::uint128_t> (1ULL) << bits_a) - 1);
}

nano_pow::uint128_t nano_pow::bit_difficulty_64 (unsigned bits_a)
{
	assert (bits_a > 0 && bits_a < 64 && "Difficulty must be greater than 0 and less than 64");
	return bit_difficulty (bits_a + 32);
}

static nano_pow::uint128_t sum (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a)
{
	nano_pow::uint128_t result (::H0 (nonce_a, solution_a[0]) + ::H1 (nonce_a, solution_a[1]));
	return result;
}

nano_pow::uint128_t nano_pow::difficulty (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a)
{
	return ::reverse (~sum (nonce_a,  solution_a));
}

static bool passes_sum (nano_pow::uint128_t const sum_a, nano_pow::uint128_t difficulty_a)
{
	auto passed (::reverse (~sum_a) > difficulty_a);
	return passed;
}

bool nano_pow::passes (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a, nano_pow::uint128_t difficulty_a)
{
	// Solution is limited to 32 + 48 bits
	assert (solution_a[0] <= std::numeric_limits<uint32_t>::max () && solution_a[1] <= (std::numeric_limits<uint64_t>::max () >> 16));
	auto passed (passes_sum (sum (nonce_a, solution_a), difficulty_a));
	return passed;
}

NP_INLINE static nano_pow::uint128_t difficulty_quick (nano_pow::uint128_t const sum_a, nano_pow::uint128_t const difficulty_inv_a)
{
	assert ((difficulty_inv_a & (difficulty_inv_a + 1)) == 0);
	return sum_a & difficulty_inv_a;
}
/**
 * Maps item_a to an index within the memory region.
 *
 * @param item_a value that needs to be pigeonholed into an index/slot.
 *        Naively is (item_a % size_a), but using bitmasking for efficiency.
 */
NP_INLINE static uint64_t slot (uint64_t const size_a, uint64_t const item_a)
{
	auto mask (size_a - 1);
	assert (((size_a & mask) == 0) && "Slab size is not a power of 2");
	return item_a & mask;
}

NP_INLINE static bool passes_quick (nano_pow::uint128_t const sum_a, nano_pow::uint128_t const difficulty_inv_a)
{
	assert ((difficulty_inv_a & (difficulty_inv_a + 1)) == 0);
	auto passed (difficulty_quick (sum_a, difficulty_inv_a) == 0);
	return passed;
}

nano_pow::cpp_driver::cpp_driver () :
difficulty_m (nano_pow::bit_difficulty (8)),
difficulty_inv (::reverse (difficulty_m))
{
	nano_pow::memory_init ();
	threads_set (std::thread::hardware_concurrency ());	
}

nano_pow::cpp_driver::~cpp_driver ()
{
	threads_set (0);
}

std::array<uint64_t, 2> nano_pow::cpp_driver::solve (std::array <uint64_t, 2> nonce)
{
	result_0 = 0;
	result_1 = 0;
	current = 0;
	this->nonce [0] = nonce [0];
	this->nonce [1] = nonce [1];
	return nano_pow::driver::solve (nonce);
}

void nano_pow::cpp_driver::dump () const
{
	std::cerr << "Hardware threads: " << std::to_string (std::thread::hardware_concurrency ()) << std::endl;
}

bool nano_pow::cpp_driver::memory_set (size_t memory)
{
	assert (memory > 0);
	assert ((memory & (memory - 1)) == 0);
	size = memory / sizeof (uint32_t);
	bool error = false;
	slab = std::unique_ptr <uint32_t, std::function <void(uint32_t*)>> (nano_pow::alloc (memory, error), [size = this->size](uint32_t * slab) { free_page_memory (slab, size); });
	if (error)
	{
		std::cerr << "Error while creating memory buffer" << std::endl;
	}
	return error;
}

void nano_pow::cpp_driver::threads_set (unsigned threads)
{
	this->threads.resize (threads);
}

size_t nano_pow::cpp_driver::threads_get () const
{
	return threads.size ();
}

void nano_pow::cpp_driver::difficulty_set (nano_pow::uint128_t difficulty_a)
{
	difficulty_inv = ::reverse (difficulty_a);
	difficulty_m = difficulty_a;
}

nano_pow::uint128_t nano_pow::cpp_driver::difficulty_get () const
{
	return difficulty_m;
}

std::string to_string_hex (uint32_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (8) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}
void nano_pow::cpp_driver::fill_impl (uint32_t const count, uint32_t const begin)
{
	//std::cerr << (std::string ("Fill ") + to_string_hex (begin) + ' ' + to_string_hex (count) + '\n');
	auto size_l (size);
	auto nonce_l (nonce);
	auto slab_l(slab.get ());
	for (uint32_t current (begin), end (current + count); current < end; ++current)
	{
		slab_l [slot (size_l, static_cast<uint64_t> (::H0 (nonce_l, current)))] = current;
	}
}

void nano_pow::cpp_driver::search_impl (size_t thread_id)
{
	xor_shift::hash prng (thread_id + 1);
	//std::cerr << (std::string ("Search ") + to_string_hex (begin) + ' ' + to_string_hex (count) + '\n');
	auto size_l (size);
	auto nonce_l (nonce);
	auto slab_l(slab.get ());
	while (result_0 == 0)
	{
		std::array<uint64_t, 2> result_l = { 0, 0 };
		for (uint32_t j (0), m (stepping); result_l[1] == 0 && j < m; ++j)
		{
			uint64_t rhs = prng.next ();
			auto hash_l (::H1 (nonce_l, rhs));
			uint64_t lhs = slab_l [slot (size_l, 0 - static_cast<uint64_t> (hash_l))];
			auto sum (::H0 (nonce_l, lhs) + hash_l);
			// Check if the solution passes through the quick path then check it through the long path
			if (!passes_quick (sum, difficulty_inv))
			{
				// Likely
			}
			else
			{
				if (passes_sum (sum, difficulty_m))
				{
					result_l = { lhs, rhs };
				}
			}
		}
		if (result_l[1] != 0)
		{
			result_0 = result_l[0];
			result_1 = result_l[1];
		}
	}
}

void nano_pow::cpp_driver::fill ()
{
	threads.execute ([this] (size_t thread_id, size_t total_threads) {
		auto count (fill_count ());
		fill_impl (count / total_threads, current.fetch_add(count / total_threads));
	});
	threads.barrier ();
}

std::array<uint64_t, 2> nano_pow::cpp_driver::search ()
{
	threads.execute ([this] (size_t thread_id, size_t total_threads) {
		search_impl (thread_id);
	});
	threads.barrier ();
	return result_get ();
}

std::array<uint64_t, 2> nano_pow::driver::solve (std::array<uint64_t, 2> nonce)
{
	std::array<uint64_t, 2> result_l = { 0, 0 };
	while (result_l[1] == 0)
	{
		fill ();
		result_l = search ();
	}
	return result_l;
}

void nano_pow::thread_pool::barrier ()
{
	std::unique_lock<std::mutex> lock (mutex);
	finish.wait (lock, [this] () { return ready == threads.size (); });
}

void nano_pow::thread_pool::resize (size_t threads)
{
	barrier ();
	{
		std::unique_lock<std::mutex> lock (mutex);
		while (this->threads.size () < threads)
		{
			this->threads.push_back (std::make_unique<std::thread> ([this, i=this->threads.size ()] () {
				loop (i);
			}));
		}
		while (this->threads.size () > threads)
		{
			auto thread (std::move (this->threads.back ()));
			ready = 0;
			start.notify_all ();
			lock.unlock ();
			thread->join ();
			lock.lock ();
			this->threads.pop_back ();
		}
	}
	barrier ();
}

void nano_pow::thread_pool::execute (std::function<void(size_t, size_t)> operation)
{
	barrier ();
	this->operation = operation;
	ready = 0;
	start.notify_all ();
}

void nano_pow::thread_pool::stop ()
{
	barrier ();
	resize (0);
	assert (ready == 0);
	assert (!operation);
}

void nano_pow::thread_pool::loop (size_t thread_id)
{
	std::unique_lock<std::mutex> lock (mutex);
	while (threads [thread_id] != nullptr)
	{
		++ready;
		finish.notify_all ();
		start.wait (lock);
		if (operation && threads [thread_id] != nullptr)
		{
			auto threads_size (threads.size ());
			lock.unlock ();
			operation (thread_id, threads_size);
			lock.lock ();
		}
	}
}

size_t nano_pow::thread_pool::size () const
{
	std::lock_guard<std::mutex> lock (mutex);
	return threads.size ();
}

uint32_t nano_pow::cpp_driver::fill_count () const
{
	auto low_fill = std::min (std::numeric_limits<uint32_t>::max () / 3, static_cast<uint32_t> (size)) * 3;
	auto critical_size (static_cast<nano_pow::uint128_t> (size * size) >= difficulty_inv + 1);
	return critical_size ? size : low_fill;
}

std::array<uint64_t, 2> nano_pow::cpp_driver::result_get ()
{
	std::array<uint64_t, 2> result_l = { result_0.load (), result_1.load () };
	return result_l;
}
