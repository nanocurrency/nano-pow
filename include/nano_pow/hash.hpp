#pragma once

#include <highwayhash/highwayhash/sip_hash.h>

#include <array>
#include <algorithm>

namespace nano_pow
{
class hash
{
public:
	virtual uint64_t operator () (uint32_t const item_a) const = 0;
	virtual void reset (std::array <uint64_t, 2> key_a) = 0;
};
class sip_hash final : public hash
{
public:
	highwayhash::HH_U64 key[2];
	void reset (std::array <uint64_t, 2> key_a) override
	{
		key [0] = key_a [0];
		key [1] = key_a [1];
	}
	uint64_t operator () (uint32_t const item_a) const override
	{
		return highwayhash::SipHash (key, reinterpret_cast<char const *> (&item_a), sizeof (item_a));
	}
};
}
