#pragma once

#include <cassert>
#include <limits>

namespace ssp_pow
{
template <typename T>
class context
{
public:
	context (T & hash_a, unsigned difficulty_a) :
	hash (hash_a),
	threshold ((1ULL << difficulty_a) - 1)
	{
		assert (difficulty_a > 0 && difficulty_a < 64 && "Difficulty must be greater than 0 and less than 64");
	}
	T & hash;
	uint64_t const threshold;
	static uint64_t constexpr lhs_or_mask { 0x1ULL << 63 };
	static uint64_t constexpr rhs_and_mask { ~lhs_or_mask };
public:
	uint64_t reduce (uint64_t const item_a) const
	{
		return item_a & threshold;
	}
	uint64_t difficulty (uint64_t const solution_a) const
	{
		auto sum (hash ((solution_a >> 32) | lhs_or_mask) + hash (solution_a & rhs_and_mask));
		return (reduce (sum) == 0) ? 0 - sum : 0;
	}
	uint64_t slot (size_t const size_a, uint64_t const item_a) const
	{
		auto mask (size_a - 1);
		assert (((size_a & mask) == 0) && "Slab size is not a power of 2");
		return item_a & mask;
	}
	uint64_t search (uint32_t * const slab_a, size_t const size_a, uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0)
	{
		auto incomplete (true);
		uint32_t lhs, rhs;
		for (uint32_t current (begin), end (current + count); incomplete && current < end; ++current)
		{
			lhs = current;
			auto hash_l (hash (current | lhs_or_mask));
			rhs = slab_a [slot (size_a, 0 - hash_l)];
			incomplete = reduce (hash_l + hash (rhs & rhs_and_mask)) != 0;
		}
		return incomplete ? 0 : (static_cast <uint64_t> (lhs) << 32) | rhs;
	}
	void fill (uint32_t * const slab_a, size_t const size_a, uint32_t const count, uint32_t const begin = 0)
	{
		for (uint32_t current (begin), end (current + count); current < end; ++current)
		{
			slab_a [slot (size_a, hash (current & rhs_and_mask))] = current;
		}
	}
};
}
