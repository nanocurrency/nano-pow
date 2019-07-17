#pragma once

#include <BLAKE2/sse/blake2.h>
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
class blake2_hash final : public hash
{
public:
	void reset (std::array <uint64_t, 2> key_a) override
	{
		blake2b_init (&state, sizeof (uint64_t));
		blake2b_update (&state, key_a.data (), sizeof (key_a));
	}
	uint64_t operator () (uint32_t const item_a) const override
	{
		uint64_t result;
		blake2b_state state_l (state);
		blake2b_update (&state_l, &item_a, sizeof (item_a));
		blake2b_final (&state_l, &result, sizeof (result));
		return result;
	}
	blake2b_state state;
};
}