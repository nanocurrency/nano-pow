#include <gtest/gtest.h>

#include <nano_pow/pow.hpp>
#include <nano_pow/cpp_driver.hpp>

TEST (context, difficulty)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	nano_pow::cpp_driver driver;
	driver.nonce = nonce;
	driver.find (0, 0, 1);
	auto difficulty (nano_pow::difficulty (nonce, driver.result));
	ASSERT_NE (0, difficulty);
	ASSERT_EQ (0xff, difficulty >> 56);
	ASSERT_TRUE (nano_pow::passes (nonce, driver.result, 0xff00'0000'0000'0000ULL));
	ASSERT_FALSE (nano_pow::passes (nonce, driver.result, 0xffff'ffff'0000'0000ULL));
}

TEST (cpp_driver, threads)
{
	nano_pow::cpp_driver driver;
	driver.threads_set (4);
	driver.threads_set (2);
	driver.threads_set (8);
	driver.threads_set (0);
}
