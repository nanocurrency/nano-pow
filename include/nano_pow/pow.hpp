#pragma once

#include <nano_pow/uint128.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

/*
 Maximum size = 2 ^ (value_size * 8) * value_size
 | Value size | Effective bitrange | Maximum size |
 | 4          | 48 - 64            | 16  GB       |
 | 5          | 64 - 80            | 5   TB       |
 | 6          | 80 - 96            | 1.5 PB       |
 */
#define NP_SIZE_STANDARD 4
#define NP_SIZE_LARGE 5
#define NP_SIZE_GIANT 6

#ifndef NP_VALUE_SIZE
#define NP_VALUE_SIZE NP_SIZE_STANDARD
#endif

namespace nano_pow
{
	// Hash function H0 sets the high order bit
	nano_pow::uint128_t H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
	// Hash function H1 clears the high order bit
	nano_pow::uint128_t H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
	nano_pow::uint128_t reverse (nano_pow::uint128_t const item_a);
	nano_pow::uint128_t bit_difficulty (unsigned bits_a);
	nano_pow::uint128_t difficulty (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a);
	bool passes (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a, nano_pow::uint128_t difficulty_a);
}

