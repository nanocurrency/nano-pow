#pragma once

#include <nano_pow/uint128.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace nano_pow
{
enum class driver_type
{
	CPP,
	OPENCL
};

class driver
{
protected:
	bool verbose{ false };
	std::atomic<bool> cancel{ false };

public:
	virtual ~driver () = default;
	virtual void difficulty_set (nano_pow::uint128_t difficulty) = 0;
	virtual nano_pow::uint128_t difficulty_get () const = 0;
	void difficulty_set_64 (uint64_t difficulty_a)
	{
		difficulty_set (nano_pow::difficulty_64_to_128 (difficulty_a));
	}
	uint64_t difficulty_get_64 () const
	{
		return nano_pow::difficulty_128_to_64 (difficulty_get ());
	}
	virtual void threads_set (unsigned threads) = 0;
	virtual size_t threads_get () const = 0;
	// Tell the driver the amount of memory to use, in bytes
	// Value must be a power of 2
	// Returns true on error
	virtual bool memory_set (size_t memory) = 0;
	// Free used memory
	virtual void memory_reset () = 0;
	virtual void dump () const = 0;
	virtual void fill () = 0;
	virtual std::array<uint64_t, 2> search () = 0;
	virtual std::array<uint64_t, 2> solve (std::array<uint64_t, 2> nonce) = 0;
	virtual driver_type type () const = 0;
	void cancel_current ()
	{
		cancel = true;
	}
	void verbose_set (bool const v)
	{
		verbose = v;
	}
};
}
