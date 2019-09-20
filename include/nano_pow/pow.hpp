#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

namespace nano_pow
{
	// Hash function H0 sets the high order bit
	uint64_t H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
	// Hash function H1 clears the high order bit
	uint64_t H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
	uint64_t reverse (uint64_t const item_a);
	uint64_t bit_difficulty (unsigned bits_a);
	uint64_t difficulty (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a);
	bool passes (std::array<uint64_t, 2> nonce_a, uint64_t const solution_a, uint64_t difficulty_a);
}

