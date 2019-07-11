#include <gtest/gtest.h>

#include <nano_pow/hash.hpp>
#include <nano_pow/pow.hpp>
#include <cpp_driver.hpp>

TEST (context, difficulty)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	nano_pow::blake2_hash hash;
	hash.reset (nonce);
	nano_pow::generator generator;
	std::array <uint32_t, 8> slab;
	nano_pow::context context (hash, nonce, slab.data (), 8, context.bit_difficulty_inv (8));
	generator.find (context, 0, 1, 1);
	auto difficulty (context.difficulty (hash, generator.result));
	ASSERT_NE (0, difficulty);
	ASSERT_EQ (0xff, difficulty >> 56);
	ASSERT_TRUE (context.passes (hash, generator.result, 0xff00'0000'0000'0000ULL));
	ASSERT_FALSE (context.passes (hash, generator.result, 0xffff'ffff'0000'0000ULL));
}

TEST (cpp_driver, threads)
{
	nano_pow::cpp_driver driver;
	driver.threads_set (4);
	driver.threads_set (2);
	driver.threads_set (8);
	driver.threads_set (0);
}
