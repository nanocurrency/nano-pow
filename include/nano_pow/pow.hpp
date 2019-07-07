#pragma once

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

#include <nano_pow/hash.hpp>

namespace nano_pow
{
	class context final
	{
	public:
		context (nano_pow::hash & hash_a, std::array<uint64_t, 2> nonce_a, uint32_t * const slab_a, size_t const size_a, uint64_t threshold_a) :
		hash (hash_a),
		nonce (nonce_a),
		slab (slab_a),
		size (size_a),
		threshold (threshold_a)
		{
		}
		nano_pow::hash & hash;
		std::array<uint64_t, 2> nonce;
		uint32_t * slab;
		size_t size;
		uint64_t threshold;
	public:
		static uint64_t bit_threshold (unsigned bits_a)
		{
			assert (bits_a > 0 && bits_a < 64 && "Difficulty must be greater than 0 and less than 64");
			return (1ULL << bits_a) - 1;
		}
		static uint64_t reduce (uint64_t const item_a, uint64_t threshold)
		{
			return item_a & threshold;
		}
		static uint64_t reverse (uint64_t const item_a)
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
		
		/**
		 * Maps item_a to an index within the memory region.
		 *
		 * @param item_a value that needs to be pigeonholed into an index/slot.
		 *        Naively is (item_a % size_a), but using bitmasking for efficiency.
		 */
		uint64_t slot (uint64_t const item_a) const
		{
			auto mask (size - 1);
			assert (((size & mask) == 0) && "Slab size is not a power of 2");
			return item_a & mask;
		}
		
		static uint64_t difficulty (nano_pow::hash & hash_a, uint64_t const threshold_a, uint64_t const solution_a)
		{
			auto sum (hash_a.H0 (solution_a >> 32) + hash_a.H1 (solution_a));
			uint64_t result (0);
			if (reduce (sum, threshold_a) == 0)
			{
				result = reverse (~sum);
			}
			return result;
		}
		
		/**
		 * Populates memory with `count` pre-images
		 *
		 * These preimages are used as the RHS to the summing problem
		 * basically does:
		 *     slab_a[hash(x) % size_a] = x
		 *
		 * @param count How many slots in slab_a to fill
		 * @param begin starting value to hash
		 */
		void fill (nano_pow::hash & hash_a, uint32_t const count, uint32_t const begin = 0)
		{
			for (uint32_t current (begin), end (current + count); current < end; ++current)
			{
				slab [slot (hash_a.H1 (current))] = current;
			}
		}
		
		/**
		 * Searches for a solution to difficulty problem
		 *
		 * Generates `count` LHS hashes and searches for associated RHS hashes already in the slab
		 *
		 * @param count How many slots in slab_a to fill
		 * @param begin starting value to hash
		 */
		uint64_t search (nano_pow::hash & hash_a, uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0)
		{
			auto incomplete (true);
			uint32_t lhs, rhs;
			for (uint32_t current (begin), end (current + count); incomplete && current < end; ++current)
			{
				lhs = current; // All the preimages have the same MSB. This is to save 4 bytes per element
				auto hash_l (hash_a.H0 (current));
				rhs = slab [slot (0 - hash_l)];
				// if the reduce is 0, then we found a solut ion!
				incomplete = reduce (hash_l + hash_a.H1 (rhs), threshold) != 0;
			}
			return incomplete ? 0 : (static_cast <uint64_t> (lhs) << 32) | rhs;
		}
	};
	class generator
	{
	public:
		bool find (nano_pow::context & context_a, unsigned ticket_a, unsigned thread, unsigned total_threads)
		{
			uint32_t last_fill (~0); // 0xFFFFFFFF
			auto found (false);
			while (ticket == ticket_a) // job identifier, if we're still working on this job
			{
				auto current_l (current.fetch_add (stepping)); // local
				if (current_l >> 32 != last_fill) // top half of current_l
				{
					// fill the memory with 1/threads preimages
					// i.e. this thread's fair share.
					auto allowance (context_a.size / total_threads);
					last_fill = current_l >> 32;
					context_a.fill (context_a.hash, allowance, last_fill * context_a.size + thread * allowance);
				}
				auto result_l (context_a.search (context_a.hash, stepping, current_l));
				if (result_l != 0)
				{
					// solution found!
					// Increment the job counter, and set the results to the found preimage
					if (ticket.fetch_add (1) == ticket_a)
					{
						result = result_l;
						found = true;
					}
				}
			}
			return found;
		}
		std::atomic<uint64_t> result { 0 };
		std::atomic<uint64_t> current { 0 };
		std::atomic<unsigned> ticket { 0 };
		static uint32_t constexpr stepping { 1024 };
	};
}

