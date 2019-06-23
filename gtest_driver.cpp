#include <gtest/gtest.h>

#include <ssp_pow/hash.hpp>
#include <ssp_pow/pow.hpp>

TEST (context, difficulty)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	ssp_pow::blake2_hash hash (nonce);
	ssp_pow::generator<ssp_pow::blake2_hash> generator;
	std::array <uint32_t, 8> slab;
	ssp_pow::context<ssp_pow::blake2_hash> context (hash, slab.data (), 8, context.bit_threshold (8));
	generator.find (context, 0, 1, 1);
	auto difficulty (context.difficulty (generator.result));
	ASSERT_NE (0, difficulty);
	ASSERT_EQ (0xff, difficulty >> 56);
}
