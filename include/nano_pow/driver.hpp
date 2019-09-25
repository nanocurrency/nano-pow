#pragma once

#include <nano_pow/uint128.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace nano_pow
{
class driver
{
protected:
	bool verbose{ false };

public:
	virtual ~driver () = default;
	virtual void difficulty_set (nano_pow::uint128_t difficulty) = 0;
	virtual nano_pow::uint128_t difficulty_get () const = 0;
	virtual void threads_set (unsigned threads) = 0;
	virtual size_t threads_get () const = 0;
	// Tell the driver the amount of memory to use, in bytes
	// Value must be a power of 2
	// Returns true on error
	virtual bool memory_set (size_t memory) = 0;
	virtual void dump () const = 0;
	virtual void fill () = 0;
	virtual std::array<uint64_t, 2> search () = 0;
	virtual std::array<uint64_t, 2> solve (std::array<uint64_t, 2> nonce) = 0;
	virtual bool tune (unsigned const count, size_t const initial_memory, size_t const initial_threads, size_t & max_memory, size_t & best_memory, size_t & best_threads, std::ostream & stream) = 0;
	virtual bool tune (unsigned const count, size_t const initial_memory, size_t const initial_threads, size_t & max_memory, size_t & best_memory, size_t & best_threads) = 0;
	void verbose_set (bool const v)
	{
		verbose = v;
	}
};
}
