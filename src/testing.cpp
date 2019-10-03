#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/opencl_driver.hpp>
#include <nano_pow/pow.hpp>

#include <gtest/gtest.h>

TEST (nano_pow, difficulty_64)
{
	ASSERT_EQ (nano_pow::reverse ((static_cast<nano_pow::uint128_t> (0x1ULL) << (4 + 32)) - 1), nano_pow::bit_difficulty_64 (4));
	ASSERT_EQ (nano_pow::reverse ((static_cast<nano_pow::uint128_t> (0x1ULL) << (16 + 32)) - 1), nano_pow::bit_difficulty_64 (16));
}

TEST (nano_pow, difficulty_128)
{
	ASSERT_EQ (nano_pow::reverse ((static_cast<nano_pow::uint128_t> (0x1ULL) << 4) - 1), nano_pow::bit_difficulty (4));
	ASSERT_EQ (nano_pow::reverse ((static_cast<nano_pow::uint128_t> (0x1ULL) << 64) - 1), nano_pow::bit_difficulty (64));
}

TEST (cpp_driver, solve)
{
	std::array<uint64_t, 2> nonce{ 0, 0 };
	nano_pow::cpp_driver driver;
	ASSERT_FALSE (driver.memory_set (1ULL << 16));
	driver.difficulty_set (nano_pow::bit_difficulty (32));
	auto result (driver.solve (nonce));
	auto difficulty (nano_pow::difficulty (nonce, result));
	ASSERT_NE (static_cast<nano_pow::uint128_t> (0), difficulty);
	auto passing_difficulty (nano_pow::bit_difficulty (32));
	ASSERT_GE (difficulty, passing_difficulty);
	ASSERT_TRUE (nano_pow::passes (nonce, result, passing_difficulty));
	auto failing_difficulty (nano_pow::bit_difficulty (70));
	ASSERT_LT (difficulty, failing_difficulty);
	ASSERT_FALSE (nano_pow::passes (nonce, result, failing_difficulty));
}

TEST (opencl_driver, solve)
{
	bool opencl_available{ true };
	std::array<uint64_t, 2> nonce{ 0, 0 };
	nano_pow::opencl_driver driver (0, 0, false);
	try
	{
		driver.initialize (0, 0);
	}
	catch (nano_pow::OCLDriverException const & err)
	{
		opencl_available = false;
		std::cerr << "OpenCL not available, skipping test" << std::endl;
	}
	if (opencl_available)
	{
		ASSERT_FALSE (driver.memory_set (1ULL << 16));
		driver.difficulty_set (nano_pow::bit_difficulty (32));
		auto result = driver.solve (nonce);
		auto difficulty (nano_pow::difficulty (nonce, result));
		ASSERT_NE (static_cast<nano_pow::uint128_t> (0), difficulty);
		ASSERT_TRUE (nano_pow::passes (nonce, result, nano_pow::bit_difficulty (32)));
		ASSERT_FALSE (nano_pow::passes (nonce, result, nano_pow::bit_difficulty (70)));
	}
}
