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
	virtual ~driver() = default;
	virtual void difficulty_set (nano_pow::uint128_t difficulty) = 0;
	virtual nano_pow::uint128_t difficulty_get () const = 0;
	void difficulty_set_64 (uint64_t difficulty_a)
	{
		auto difficulty (static_cast<nano_pow::uint128_t> (0xffffffffU) << 96 | static_cast<nano_pow::uint128_t> (difficulty_a) << 32);
		difficulty_set (difficulty);
	}
	uint64_t difficulty_get_64 () const
	{
		return static_cast<uint64_t> (difficulty_get () >> 32);
	}
	virtual void threads_set (unsigned threads) = 0;
	virtual size_t threads_get () const = 0;
	// Tell the driver the amount of memory to use, in bytes
	// Value must be a power of 2
	// Returns true on error
	virtual bool memory_set (size_t memory) = 0;
	virtual void dump () const = 0;
	virtual void fill () = 0;
	virtual std::array<uint64_t, 2> search () = 0;
	virtual std::array<uint64_t, 2> solve (std::array<uint64_t, 2> nonce);
	void verbose_set(bool const v) { verbose = v; }
};
}
