#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/pow.hpp>

#include <gtest/gtest.h>

TEST (context, difficulty)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	nano_pow::cpp_driver driver;
	driver.solve (nonce);
	auto difficulty (nano_pow::difficulty (nonce, driver.result_get ()));
	ASSERT_NE (static_cast<nano_pow::uint128_t> (0), difficulty);
	ASSERT_EQ (static_cast<nano_pow::uint128_t> (0xff), difficulty >> 56);
	ASSERT_TRUE (nano_pow::passes (nonce, driver.result_get (), static_cast<nano_pow::uint128_t> (0xff00'0000'0000'0000ULL) << 64));
	ASSERT_FALSE (nano_pow::passes (nonce, driver.result_get (), static_cast<nano_pow::uint128_t> (0xffff'ffff'0000'0000ULL) << 64));
}

TEST (cpp_driver, threads)
{
	nano_pow::cpp_driver driver;
	driver.threads_set (4);
	driver.threads_set (2);
	driver.threads_set (8);
	driver.threads_set (0);
}
