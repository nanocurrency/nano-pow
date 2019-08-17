#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

#include <highwayhash/highwayhash/sip_hash.h>

namespace nano_pow
{
	class context final
	{
	public:
		context (std::array<uint64_t, 2> nonce_a, uint32_t * const slab_a, size_t const size_a, std::nullptr_t, uint64_t difficulty_a) :
		nonce (nonce_a),
		slab (slab_a),
		size (size_a),
		difficulty_inv (reverse (difficulty_a)),
		difficulty_m (difficulty_a)
		{
		}
		std::array<uint64_t, 2> nonce;
		uint32_t * slab;
		size_t size;
		uint64_t difficulty_inv;
		uint64_t difficulty_m;
		static uint64_t constexpr lhs_or_mask { 0x1ULL << 63 };
		static uint64_t constexpr rhs_and_mask { 0xffff'ffff };
	public:
		static inline uint64_t hash (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
		{
			highwayhash::SipHashState::Key nonce = { nonce_a [0], nonce_a [1] };
			return highwayhash::SipHash (nonce, reinterpret_cast<char const *> (&item_a), sizeof (item_a));
		}
		// Hash function H0 sets the high order bit
		static inline uint64_t H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
		{
			return hash (nonce_a, lhs_or_mask | item_a);
		}
		// Hash function H1 clears the high order bit
		static inline uint64_t H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a)
		{
			return hash (nonce_a, rhs_and_mask & item_a);
		}
		static inline uint64_t sum (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a)
		{
			auto result (H0 (nonce_a, solution_a >> 32) + H1 (nonce_a, solution_a));
			return result;
		}
	public:
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
		static uint64_t bit_difficulty (unsigned bits_a)
		{
			assert (bits_a > 0 && bits_a < 64 && "Difficulty must be greater than 0 and less than 64");
			return reverse ((1ULL << bits_a) - 1);
		}
			
		static uint64_t difficulty (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a)
		{
			return reverse (~sum (nonce_a,  solution_a));
		}
		static bool passes (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a, uint64_t difficulty_a)
		{
			auto passed (reverse (~sum (nonce_a, solution_a)) > difficulty_a);
			return passed;
		}
			
		static uint64_t difficulty_quick (uint64_t const sum_a, uint64_t const difficulty_inv_a)
		{
			assert ((difficulty_inv_a & (difficulty_inv_a + 1)) == 0);
			return sum_a & difficulty_inv_a;
		}
		static bool passes_quick (uint64_t const sum_a, uint64_t const difficulty_inv_a)
		{
			assert ((difficulty_inv_a & (difficulty_inv_a + 1)) == 0);
			auto passed (difficulty_quick (sum_a, difficulty_inv_a) == 0);
			return passed;
		}
		static bool passes_sum (uint64_t const sum_a, uint64_t threshold_a)
		{
			auto passed (reverse (~sum_a) > threshold_a);
			return passed;
		}

		/**
		 * Maps item_a to an index within the memory region.
		 *
		 * @param item_a value that needs to be pigeonholed into an index/slot.
		 *        Naively is (item_a % size_a), but using bitmasking for efficiency.
		 */
		uint64_t slot (uint64_t const size_a, uint64_t const item_a) const
		{
			auto mask (size_a - 1);
			assert (((size_a & mask) == 0) && "Slab size is not a power of 2");
			return item_a & mask;
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
		void fill (uint32_t const count, uint32_t const begin = 0)
		{
			auto size_l (size);
			auto nonce_l (nonce);
			for (uint32_t current (begin), end (current + count); current < end; ++current)
			{
				slab [slot (size_l, H0 (nonce_l, current))] = current;
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
		uint64_t search (uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0)
		{
			auto incomplete (true);
			uint32_t lhs, rhs;
			auto nonce_l (nonce);
			auto size_l (size);
			for (uint32_t current (begin), end (current + count); incomplete && current < end; ++current)
			{
				rhs = current;
				auto hash_l (H1 (nonce_l, rhs));
				lhs = slab [slot (size_l, 0 - hash_l)];
				auto sum (H0 (nonce_l, lhs) + hash_l);
				// Check if the solution passes through the quick path then check it through the long path
				incomplete = !passes_quick (sum, difficulty_inv) || !passes_sum (sum, difficulty_m);
			}
			return incomplete ? 0 : (static_cast <uint64_t> (lhs) << 32) | rhs;
		}
	};
	class generator
	{
	public:
		bool find (nano_pow::context & context_a, unsigned ticket_a, size_t thread, size_t total_threads)
		{
			uint32_t last_fill (~0); // 0xFFFFFFFF
			auto found (false);
			while (ticket == ticket_a) // job identifier, if we're still working on this job
			{
				uint64_t current_l (current.fetch_add (stepping));
				uint32_t fill_iteration (static_cast<uint32_t> (current_l >> 32));
				uint32_t search_iteration (static_cast<uint32_t> (current_l >> 0));
				if (fill_iteration != last_fill) // top half of current_l
				{
					// fill the memory with 1/threads preimages
					// i.e. this thread's fair share.
					uint32_t allowance (static_cast<uint32_t> (context_a.size / total_threads));
					last_fill = fill_iteration;
					uint32_t fill_count (static_cast<uint32_t> (last_fill * context_a.size + thread * allowance));
					context_a.fill (allowance, fill_count);
				}
				auto result_l (context_a.search (stepping, search_iteration));
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

