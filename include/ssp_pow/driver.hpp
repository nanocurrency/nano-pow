#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ssp_pow
{
class driver
{
public:
	virtual ~driver() = default;
	virtual void threshold_set (uint64_t threshold) = 0;
	virtual void threads_set (unsigned threads) = 0;
	virtual void lookup_set (size_t lookup) = 0;
	virtual uint64_t solve (std::array<uint64_t, 2> nonce) = 0;
};
}