#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nano_pow
{
class driver
{
public:
	virtual ~driver() = default;
	virtual void difficulty_set (uint64_t difficulty) = 0;
	virtual uint64_t difficulty_get () const = 0;
	virtual void threads_set (unsigned threads) = 0;
	virtual size_t threads_get () const = 0;
	// Tell the driver the amount of memory to use, in bytes
	// Value must be a power of 2
	// Returns true on error
	virtual bool memory_set (size_t memory) = 0;
	// Places result in given variable
	// Returns true on error
	virtual bool solve (std::array<uint64_t, 2> nonce, uint64_t & result) = 0;
	virtual bool ok () const = 0;
	virtual void dump () const = 0;
};
}
