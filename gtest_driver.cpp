#include <gtest/gtest.h>

#include <ssp_pow/hash.hpp>
#include <ssp_pow/pow.hpp>
#include <cpp_driver.hpp>

TEST (context, difficulty)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	ssp_pow::blake2_hash hash;
	hash.reset (nonce);
	ssp_pow::generator generator;
	std::array <uint32_t, 8> slab;
	ssp_pow::context context (hash, nonce, slab.data (), 8, context.bit_threshold (8));
	generator.find (context, 0, 1, 1);
	auto difficulty (context.difficulty (hash, generator.result));
	ASSERT_NE (0, difficulty);
	ASSERT_EQ (0xff, difficulty >> 56);
}

TEST (cpp_driver, threads)
{
	ssp_pow::cpp_driver driver;
	driver.threads_set (4);
	driver.threads_set (2);
	driver.threads_set (8);
	driver.threads_set (0);
}
