#pragma once

#include <nano_pow/uint128.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>

namespace nano_pow
{
// Hash function H0 sets the high order bit
nano_pow::uint128_t H0 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
// Hash function H1 clears the high order bit
nano_pow::uint128_t H1 (std::array<uint64_t, 2> nonce_a, uint64_t const item_a);
nano_pow::uint128_t reverse (nano_pow::uint128_t const item_a);
nano_pow::uint128_t bit_difficulty (unsigned bits_a);
nano_pow::uint128_t bit_difficulty_64 (unsigned bits_a);
nano_pow::uint128_t difficulty (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a);
bool passes (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a, nano_pow::uint128_t difficulty_a);
bool passes_64 (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> const solution_a, uint64_t difficulty_a);
}
