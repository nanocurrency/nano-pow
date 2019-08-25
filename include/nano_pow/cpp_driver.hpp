#pragma once

#include <nano_pow/driver.hpp>

#include <nano_pow/pow.hpp>

#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace nano_pow
{
	class thread_pool
	{
	public:
		void resize (size_t);
		void barrier ();
		void execute (std::function<void(size_t, size_t)>);
		void stop ();
		size_t size () const;
	private:
		void loop (size_t thread_id);
		size_t ready { 0 };
		std::function<void(size_t, size_t)> operation;
		std::vector<std::unique_ptr<std::thread>> threads;
		std::condition_variable finish;
		std::condition_variable start;
		mutable std::mutex mutex;
	};
	class cpp_driver : public driver
	{
	public:
		cpp_driver ();
		~cpp_driver ();
		void difficulty_set (uint64_t difficulty) override;
		uint64_t difficulty_get () const override;
		void threads_set (unsigned threads) override;
		size_t threads_get () const override;
		bool memory_set (size_t memory) override;
		uint64_t solve (std::array<uint64_t, 2> nonce) override;
		void dump () const override;
		void find (size_t thread, size_t total_threads);
		std::atomic<uint64_t> result { 0 };
	private:
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
		void fill (uint32_t const count, uint32_t const begin = 0);
		
		/**
		 * Searches for a solution to difficulty problem
		 *
		 * Generates `count` LHS hashes and searches for associated RHS hashes already in the slab
		 *
		 * @param count How many slots in slab_a to fill
		 * @param begin starting value to hash
		 */
		uint64_t search (uint32_t const count = std::numeric_limits<uint32_t>::max (), uint32_t const begin = 0);
		std::atomic<uint64_t> current { 0 };
		static uint32_t constexpr stepping { 1024 };
		thread_pool threads;
		std::condition_variable condition;
		mutable std::mutex mutex;
		uint64_t difficulty_m;
		uint64_t difficulty_inv;
		size_t size { 0 };
		uint32_t * slab { nullptr };
	public:
		std::array<uint64_t, 2> nonce { { 0, 0 } };
	};
}
