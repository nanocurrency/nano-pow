#include <nano_pow/pow.hpp>
#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/opencl_driver.hpp>
#include <gtest/gtest.h>
#include <fstream>
#include <cxxopts.hpp>

namespace
{
std::string to_string_hex (uint32_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (8) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}
std::string to_string_hex64 (uint64_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}
std::string to_string_solution (std::array<uint64_t, 2> nonce_a, uint64_t threshold_a, uint64_t solution_a)
{
	auto lhs (solution_a >> 32);
	auto lhs_hash (nano_pow::H0 (nonce_a, lhs));
	auto rhs (solution_a & 0xffffffffULL);
	auto rhs_hash (nano_pow::H1 (nonce_a, rhs));
	auto sum (lhs_hash + rhs_hash);
	std::ostringstream oss;
	oss << "H0(" << to_string_hex (static_cast<uint32_t> (lhs)) << ")+H1(" << to_string_hex (static_cast<uint32_t> (rhs)) << ")=" << to_string_hex64 (sum) << " " << to_string_hex64 (nano_pow::difficulty (nonce_a, solution_a));
	return oss.str ();
}
uint64_t profile (nano_pow::driver & driver_a, unsigned threads, uint64_t difficulty, uint64_t memory, unsigned count)
{
	std::cout << "Initializing driver" << std::endl;
	if (threads != 0)
	{
		driver_a.threads_set (threads);
	}
	driver_a.difficulty_set (difficulty);
	if (driver_a.memory_set(memory))
	{
		exit (1);
	}
	std::cout << "Starting profile" << std::endl;
	uint64_t total_time (0);
	try {
		for (auto i(0UL); i < count; ++i)
		{
			auto start(std::chrono::steady_clock::now());
			std::array <uint64_t, 2> nonce{ i + 1, 0 };
			auto result = driver_a.solve(nonce);
			auto search_time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
			total_time += search_time;
			std::cerr << to_string_solution (nonce, driver_a.difficulty_get (), result) << " solution ms: " << std::to_string (search_time) << std::endl;
		}
	}
	catch (nano_pow::OCLDriverException const& err) {
		std::cerr << "Failed to profile OpenCL" << std::endl;
		err.print (std::cerr);
	}
	uint64_t average (total_time / count);
	std::cout << "Average solution time: " << std::to_string (average) << " ms" << std::endl;
	return average;
}
uint64_t profile_validate (uint64_t count)
{
	std::array<uint64_t, 2> nonce = { 0, 0 };
	uint64_t random_difficulty{ 0xffffffc000000000 };
	std::cout << "Starting validation profile" << std::endl;
	auto start (std::chrono::steady_clock::now ());
	bool valid{ false };
	for (uint64_t i (0); i < count; ++i)
	{
		valid = nano_pow::passes (nonce, i, random_difficulty);
	}
	auto total_time (std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::steady_clock::now () - start).count ());
	std::ostringstream oss (valid ? "true" : "false"); // IO forces compiler to not dismiss the variable
	uint64_t average (total_time / count);
	std::cout << "Average validation time: " << std::to_string (average) << " ns" << std::endl;
	return average;
}
}

int main (int argc, char **argv)
{
	cxxopts::Options options ("nano_pow_driver", "Command line options");
	options.add_options ()
	("driver", "Specify which test driver to use", cxxopts::value<std::string> ()->default_value ("cpp"))
	("d,difficulty", "Solution difficulty 1-64 default: 52", cxxopts::value<unsigned> ()->default_value ("52"))
	("t,threads", "Number of device threads to use to find solution", cxxopts::value<unsigned> ())
	("l,lookup", "Scale of lookup table (N). Table contains 2^N entries, N defaults to (difficulty/2 + 1)", cxxopts::value<unsigned> ())
	("c,count", "Specify how many problems to solve, default 16", cxxopts::value<unsigned> ()->default_value ("16"))
	("operation", "Specify which driver operation to perform", cxxopts::value<std::string> ()->default_value ("gtest"))
	("platform", "Defines the <platform> for OpenCL driver", cxxopts::value<unsigned short> ())
	("device", "Defines <device> for OpenCL driver", cxxopts::value<unsigned short> ())
	("v,verbose", "Display more messages")
	("h,help", "Print this message");
	auto parsed = options.parse(argc, argv);
	int result (1);
	try
	{
		if (parsed.count("help"))
		{
			std::cout << options.help() << std::endl;
		}
		else
		{
			std::cout << "Initializing driver" << std::endl;
			std::unique_ptr<nano_pow::driver> driver{ nullptr };
			auto driver_type(parsed["driver"].as<std::string>());
			if (driver_type == "cpp")
			{
				driver = std::make_unique<nano_pow::cpp_driver>();
			}
			else if (driver_type == "opencl")
			{
				unsigned short platform(0);
				if (parsed.count("platform"))
				{
					platform = parsed["platform"].as<unsigned short>();
				}
				unsigned short device(0);
				if (parsed.count("device"))
				{
					device = parsed["device"].as<unsigned short>();
				}
				driver = std::make_unique<nano_pow::opencl_driver>(platform, device);
			}
			else
			{
				std::cerr << "Invalid driver. Available: {cpp, opencl}" << std::endl;
			}
			if (driver != nullptr && result)
			{
				std::cout << "Driver: " << driver_type << std::endl;
				driver->verbose_set(parsed.count("verbose") == 1);
				auto difficulty(parsed["difficulty"].as<unsigned>());
				auto lookup(difficulty / 2 + 1);
				if (parsed.count("lookup"))
				{
					lookup = parsed["lookup"].as <unsigned>();
				}
				auto lookup_entries(1ULL << lookup);
				auto count(parsed["count"].as<unsigned>());
				unsigned threads(0);
				if (parsed.count("threads"))
				{
					threads = parsed["threads"].as <unsigned>();
				}
				auto operation(parsed["operation"].as<std::string>());

				if (operation == "gtest")
				{
					testing::InitGoogleTest(&argc, argv);
					result = RUN_ALL_TESTS();
				}
				else if (operation == "dump")
				{
					if (driver != nullptr)
						driver->dump();
				}
				else if (operation == "profile")
				{
					if (driver != nullptr)
					{
						std::string threads_l(std::to_string(threads != 0 ? threads : driver->threads_get()));
						std::cout << "Profiling threads: " << threads_l << " lookup: " << std::to_string((1ULL << lookup) / 1024 * 4) << "kb threshold: " << to_string_hex64((1ULL << difficulty) - 1) << std::endl;
						profile(*driver, threads, nano_pow::reverse ((1ULL << difficulty) - 1), lookup_entries * sizeof(uint32_t), count);
					}
				}
				else if (operation == "profile_validation")
					profile_validate (count);
				else {
					std::cerr << "Invalid operation. Available: {gtest, dump, profile, profile_validation}" << std::endl;
					result = -1;
				}
			}
			else
			{
				std::cerr << "Driver errored" << std::endl;
			}
		}
	}
	catch (cxxopts::OptionException const & err)
	{
		std::cerr << err.what () << std::endl;
	}
	catch (nano_pow::OCLDriverException const& err) {
		std::cerr << "OpenCL error" << std::endl;
		err.print(std::cerr);
	}
	return result;
}
