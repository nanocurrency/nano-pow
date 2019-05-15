#pragma once

#include <cassert>
#include <limits>

#include <BLAKE2/sse/blake2.h>
#include <highwayhash/highwayhash/sip_hash.h>

namespace ssp_pow
{
inline uint64_t sip_hash (void const * const nonce_a, uint64_t const item_a)
{
	return highwayhash::SipHash (*reinterpret_cast<highwayhash::HH_U64 const (*) [2]> (nonce_a), reinterpret_cast<char const *> (&item_a), sizeof (item_a));
}
inline uint64_t blake2_hash (void const * const nonce_a, uint64_t const item_a)
{
	uint64_t result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result));
	blake2b_update (&state, nonce_a, sizeof (uint64_t) * 2);
	blake2b_update (&state, &item_a, sizeof (item_a));
	blake2b_final (&state, &result, sizeof (result));
	return result;
}
inline uint64_t hash (void const * const nonce_a, uint64_t const item_a)
{
	return blake2_hash (nonce_a, item_a);
}
class context
{
public:
	context (unsigned difficulty_a) :
	threshold ((1ULL << difficulty_a) - 1)
	{
		assert (difficulty_a > 0 && difficulty_a < 64 && "Difficulty must be greater than 0 and less than 64");
	}
	uint64_t const threshold;
	static uint64_t constexpr lhs_or_mask { 0x1ULL << 63 };
	static uint64_t constexpr rhs_and_mask { ~lhs_or_mask };
public:
	uint64_t reduce (uint64_t const item_a) const
	{
		return item_a & threshold;
	}
	uint64_t difficulty (void const * const nonce_a, uint64_t const solution_a) const
	{
		auto sum (hash (nonce_a, (solution_a >> 32) | lhs_or_mask) + hash (nonce_a, solution_a & rhs_and_mask));
		return (reduce (sum) == 0) ? 0 - sum : 0;
	}
	uint64_t slot (size_t const size_a, uint64_t const item_a) const
	{
		auto mask (size_a - 1);
		assert (((size_a & mask) == 0) && "Slab size is not a power of 2");
		return item_a & mask;
	}
	uint64_t search (uint32_t * const slab_a, size_t const size_a, void const * const nonce_a, uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0)
	{
		auto incomplete (true);
		uint32_t lhs;
		uint32_t rhs;
		for (uint32_t current (begin), end (current + count); incomplete && current < end; ++current)
		{
			lhs = current;
			auto hash_l (hash (nonce_a, current | lhs_or_mask));
			rhs = slab_a [slot (size_a, 0 - hash_l)];
			incomplete = reduce (hash_l + hash (nonce_a, rhs & rhs_and_mask)) != 0;
		}
		return incomplete ? 0 : (static_cast <uint64_t> (lhs) << 32) | rhs;
	}
	void fill (uint32_t * const slab_a, size_t const size_a, void const * const nonce_a, uint32_t const count, uint32_t const begin = 0)
	{
		for (uint32_t current (begin), end (current + count); current < end; ++current)
		{
			slab_a [slot (size_a, hash (nonce_a, current & rhs_and_mask))] = current;
		}
	}
};
}
