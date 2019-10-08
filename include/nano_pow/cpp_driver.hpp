#pragma once

#include <nano_pow/driver.hpp>
#include <nano_pow/memory.hpp>
#include <nano_pow/pow.hpp>
#include <nano_pow/xoroshiro128starstar.hpp>

#include <array>
#include <condition_variable>
#include <functional>
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
	size_t ready{ 0 };
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
	void difficulty_set (nano_pow::uint128_t difficulty_a) override;
	nano_pow::uint128_t difficulty_get () const override;
	void threads_set (unsigned threads) override;
	size_t threads_get () const override;
	bool memory_set (size_t memory) override;
	void memory_reset () override;
	std::array<uint64_t, 2> solve (std::array<uint64_t, 2> nonce) override;
	void dump () const override;
	driver_type type () const override
	{
		return driver_type::CPP;
	}

private:
	/*
	 * Populates memory with `count` pre-images
	 *
	 * These preimages are used as the RHS to the summing problem
	 * basically does:
	 *     slab_a[hash(x) % size_a] = x
	 *
	 * @param count How many buckets to fill in slab_a
	 * @param begin starting value to hash
	 */
	void fill_impl (uint64_t const count, uint64_t const begin = 0);
	void fill () override;

	/*
	 * Searches for a solution to difficulty problem
	 *
	 * Generates `count` LHS hashes and searches for associated RHS hashes already in the slab
	 *
	 * @param count How many buckets to fill in slab_a
	 * @param begin starting value to hash
	 */
	void search_impl (size_t thread_id);
	std::array<uint64_t, 2> search () override;
	std::atomic<uint64_t> current{ 0 };
	static uint32_t constexpr stepping{ 1024 };
	thread_pool threads;
	std::condition_variable condition;
	mutable std::mutex mutex;
	nano_pow::uint128_t difficulty_m;
	nano_pow::uint128_t difficulty_inv;
	uint64_t fill_count () const;
	size_t size{ 0 };
	std::unique_ptr<uint32_t, std::function<void(uint32_t *)>> slab{ nullptr, [](uint32_t *) {} };
	std::atomic<uint64_t> result_0{ 0 };
	std::atomic<uint64_t> result_1{ 0 };

public:
	std::array<uint64_t, 2> nonce{ { 0, 0 } };
	std::array<uint64_t, 2> result_get ();
};
} // namespace nano_pow
