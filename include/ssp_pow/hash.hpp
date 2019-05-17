#pragma once

#include <BLAKE2/sse/blake2.h>
#include <highwayhash/highwayhash/sip_hash.h>

#include <array>
#include <algorithm>

namespace ssp_pow
{
class sip_hash final
{
public:
	highwayhash::HH_U64 key[2];
	inline uint64_t operator () (uint64_t const item_a) const
	{
		return highwayhash::SipHash (key, reinterpret_cast<char const *> (&item_a), sizeof (item_a));
	}
};
class blake2_hash final
{
public:
	blake2_hash (std::array <uint64_t, 2> key_a)
	{
		blake2b_init (&state, sizeof (uint64_t));
		blake2b_update (&state, key_a.data (), sizeof (key_a));
	}
	inline uint64_t operator () (uint64_t const item_a) const
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
