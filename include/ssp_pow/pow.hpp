#pragma once

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

namespace ssp_pow
{
template <typename T>
class context final
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
	static uint64_t constexpr rhs_and_mask { 0x7fff'ffff };
public:
	uint64_t reduce (uint64_t const item_a) const
	{
		return item_a & threshold;
	}
	uint64_t reverse (uint64_t const item_a) const
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
	uint64_t difficulty (uint64_t const solution_a) const
	{
		auto sum (hash ((solution_a >> 32) | lhs_or_mask) + hash (solution_a & rhs_and_mask));
		uint64_t result (0);
		if (reduce (sum) == 0)
		{
			result = reverse (~sum);
		}
		return result;
	}

	/**
	 * Maps item_a to an index within the memory region.
	 * @param size_a size of memory region in bytes. Must be a power of 2
	 * @param item_a value that needs to be pigeonholed into an index/slot.
	 *        Naively is (item_a % size_a), but using bitmasking for efficiency.
	 */
	uint64_t slot (size_t const size_a, uint64_t const item_a) const
	{
		auto mask (size_a - 1);
		assert (((size_a & mask) == 0) && "Slab size is not a power of 2");
		return item_a & mask;
	}

	/**
	 *
	 * @param slab_a pointer to memory region
	 * @param size_a size of memory region
	 */
	uint64_t search (uint32_t * const slab_a, size_t const size_a, uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0)
	{
		auto incomplete (true);
		uint32_t lhs, rhs;
		for (uint32_t current (begin), end (current + count); incomplete && current < end; ++current)
		{
			lhs = current; // All the preimages have the same MSB. This is to save 4 bytes per element
			auto hash_l (hash (current | lhs_or_mask));
			rhs = slab_a [slot (size_a, 0 - hash_l)];
			// if the reduce is 0, then we found a solution!
			incomplete = reduce (hash_l + hash (rhs & rhs_and_mask)) != 0;
		}
		return incomplete ? 0 : (static_cast <uint64_t> (lhs) << 32) | rhs;
	}

	/**
	 * Populates memory with `count` pre-images
	 *
	 * basically does:
	 *     slab_a[hash(x) % size_a] = x
	 *
	 * @param slab_a pointer to memory region
	 * @param size_a size of memory region
	 * @param count How many slots in slab_a to fill
	 * @param begin starting value to hash
	 */
	void fill (uint32_t * const slab_a, size_t const size_a, uint32_t const count, uint32_t const begin = 0)
	{
		for (uint32_t current (begin), end (current + count); current < end; ++current)
		{
			slab_a [slot (size_a, hash (current & rhs_and_mask))] = current;
		}
	}
};
template <typename T>
class generator
{
public:
	void find (ssp_pow::context<T> & context_a, uint32_t * const slab_a, size_t const size_a, unsigned ticket_a, unsigned thread, unsigned total_threads)
	{
		uint32_t last_fill (~0); // 0xFFFFFFFF
		while (ticket == ticket_a) // job identifier, if we're still working on this job
		{
			auto current_l (current.fetch_add (stepping)); // local
			if (current_l >> 32 != last_fill) // top half of current_l
			{
				// fill the memory with 1/threads preimages
				// i.e. this thread's fair share.
				auto allowance (size_a / total_threads);
				last_fill = current_l >> 32;
				context_a.fill (slab_a, size_a,
					allowance,
					last_fill * size_a + thread * allowance );
			}
			auto result_l (context_a.search (slab_a, size_a, stepping, current_l));
			if (result_l != 0)
			{
				// solution found!
				// Increment the job counter, and set the results to the found preimage
				if (ticket.fetch_add (1) == ticket_a)
				{
					result = result_l;
				}
			}
		}
	}
	std::atomic<uint64_t> result { 0 };
	std::atomic<uint64_t> current { 0 };
	std::atomic<unsigned> ticket { 0 };
	static uint32_t constexpr stepping { 1024 };
};
}
