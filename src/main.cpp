#include <nano_pow/conversions.hpp>
#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/opencl_driver.hpp>
#include <nano_pow/tuning.hpp>

#include <cxxopts.hpp>
#include <gtest/gtest.h>

#include <fstream>

namespace
{
std::string to_string_hex64 (uint64_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}
std::string to_string_hex128 (nano_pow::uint128_t value_a)
{
	return to_string_hex64 (static_cast<uint64_t> (value_a >> 64)) + to_string_hex64 (static_cast<uint64_t> (value_a));
}
std::string to_string_solution (std::array<uint64_t, 2> nonce_a, std::array<uint64_t, 2> solution_a)
{
	auto lhs (solution_a[0]);
	auto lhs_hash (nano_pow::H0 (nonce_a, lhs));
	auto rhs (solution_a[1]);
	auto rhs_hash (nano_pow::H1 (nonce_a, rhs));
	auto sum (lhs_hash + rhs_hash);
	auto difficulty (nano_pow::difficulty (nonce_a, solution_a));
	std::ostringstream oss;
	oss << "H0(" << to_string_hex64 (lhs) << ")+H1(" << to_string_hex64 (rhs) << ")=" << to_string_hex128 (sum) << " " << to_string_hex128 (difficulty);
	return oss.str ();
}
uint64_t profile (nano_pow::driver & driver_a, unsigned threads, nano_pow::uint128_t difficulty, uint64_t memory, unsigned count)
{
	std::cout << "Initializing driver" << std::endl;
	if (threads != 0)
	{
		driver_a.threads_set (threads);
	}
	driver_a.difficulty_set (difficulty);
	if (driver_a.memory_set (memory))
	{
		std::cerr << "Failed to allocate " << nano_pow::to_megabytes (memory) << "MB" << std::endl;
		exit (1);
	}
	std::cout << "Starting profile" << std::endl;
	uint64_t total_time (0);
	try
	{
		for (auto i (0UL); i < count; ++i)
		{
			auto start (std::chrono::steady_clock::now ());
			std::array<uint64_t, 2> nonce{ i + 1, 0 };
			auto result = driver_a.solve (nonce);
			auto search_time (std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now () - start).count ());
			total_time += search_time;
			std::cout << to_string_solution (nonce, result) << " solution ms: " << std::to_string (search_time) << std::endl;
		}
	}
	catch (nano_pow::OCLDriverException const & err)
	{
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
	uint64_t difficulty{ 0xffffffc000000000 };
	std::cout << "Starting validation profile" << std::endl;
	auto start (std::chrono::steady_clock::now ());
	bool valid{ false };
	for (uint64_t i (0); i < count; ++i)
	{
		valid = nano_pow::passes (nonce, { i, i }, difficulty);
	}
	std::ostringstream oss (valid ? "true" : "false"); // IO forces compiler to not dismiss the variable
	auto total_time (std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::steady_clock::now () - start).count ());
	uint64_t average (total_time / count);
	std::cout << "Average validation time: " << std::to_string (average) << " ns (" << std::to_string (static_cast<unsigned> (count * 1e9 / total_time)) << " validations/s)" << std::endl;
	return average;
}
void tune (nano_pow::driver * driver_a, nano_pow::uint128_t difficulty, unsigned const count, size_t const initial_threads, size_t const initial_memory)
{
	driver_a->difficulty_set (difficulty);
	if (driver_a->type () == nano_pow::driver_type::CPP)
	{
		size_t best_memory{ 0 };
		if (!nano_pow::tune (*reinterpret_cast<nano_pow::cpp_driver *> (driver_a), count, initial_memory, initial_threads, best_memory, std::cerr))
		{
			std::cerr << "Tuning results:\nRecommended memory\t" << nano_pow::to_megabytes (best_memory) << "MB" << std::endl;
		}
	}
	else if (driver_a->type () == nano_pow::driver_type::OPENCL)
	{
		size_t max_memory{ 0 }, best_memory{ 0 }, best_threads{ 0 };
		if (!nano_pow::tune (*reinterpret_cast<nano_pow::opencl_driver *> (driver_a), count, initial_memory, initial_threads, max_memory, best_memory, best_threads, std::cerr))
		{
			std::cerr << "Tuning results:\nMaximum memory\t\t" << nano_pow::to_megabytes (max_memory) << "MB\nRecommended memory\t" << nano_pow::to_megabytes (best_memory) << "MB\nRecommended threads\t" << best_threads << std::endl;
		}
	}
	else
	{
		std::cerr << "Unrecognized driver" << std::endl;
	}
}
}

int main (int argc, char ** argv)
{
	cxxopts::Options options ("nano_pow_driver", "Command line options");
	options.add_options ()
	// clang-format off
		("driver", "Specify which test driver to use", cxxopts::value<std::string>()->default_value("cpp"), "cpp|opencl")
		("operation", "Specify which driver operation to perform", cxxopts::value<std::string>()->default_value("gtest"), "gtest|dump|profile|profile_validation|tune")
		("d,difficulty", "Solution difficulty 1-127 default: 52", cxxopts::value<unsigned>()->default_value("52"))
		("t,threads", "Number of device threads to use to find solution", cxxopts::value<unsigned>())
		("l,lookup", "Scale of lookup table (N). Table contains 2^N entries, N defaults to (difficulty/2 + 1)", cxxopts::value<unsigned>())
		("c,count", "Specify how many problems to solve, default 16", cxxopts::value<unsigned>()->default_value("16"))
		("platform", "Defines the <platform> for OpenCL driver", cxxopts::value<unsigned short>())
		("device", "Defines <device> for OpenCL driver", cxxopts::value<unsigned short>())
		("v,verbose", "Display more messages")
		("h,help", "Print this message");
	// clang-format on
	int result (1);
	try
	{
		auto parsed = options.parse (argc, argv);
		if (parsed.count ("help"))
		{
			std::cout << options.help () << std::endl;
		}
		else
		{
			auto operation (parsed["operation"].as<std::string> ());
			std::cout << "Initializing driver" << std::endl;
			std::unique_ptr<nano_pow::driver> driver{ nullptr };
			auto driver_type (parsed["driver"].as<std::string> ());
			if (driver_type == "cpp")
			{
				driver = std::make_unique<nano_pow::cpp_driver> ();
			}
			else if (driver_type == "opencl")
			{
				unsigned short platform (0);
				if (parsed.count ("platform"))
				{
					platform = parsed["platform"].as<unsigned short> ();
				}
				unsigned short device (0);
				if (parsed.count ("device"))
				{
					device = parsed["device"].as<unsigned short> ();
				}
				bool initialize{ operation != "dump" };
				driver = std::make_unique<nano_pow::opencl_driver> (platform, device, initialize);
			}
			else
			{
				std::cerr << "Invalid driver. Available: {cpp, opencl}" << std::endl;
			}
			if (driver != nullptr && result)
			{
				std::cout << "Driver: " << driver_type << std::endl;
				driver->verbose_set (parsed.count ("verbose") == 1);
				auto difficulty (parsed["difficulty"].as<unsigned> ());
				if (difficulty < 1 || difficulty > 127)
				{
					std::cerr << "Incorrect difficulty" << std::endl;
					return -1;
				}
				auto lookup (std::min (static_cast<unsigned> (32), difficulty / 2 + 1));
				if (parsed.count ("lookup"))
				{
					lookup = parsed["lookup"].as<unsigned> ();
				}
				if (lookup < 1 || lookup > 32)
				{
					std::cerr << "Incorrect lookup" << std::endl;
					return -1;
				}
				auto lookup_entries (nano_pow::lookup_to_entries (lookup));
				auto count (parsed["count"].as<unsigned> ());
				unsigned threads (0);
				if (parsed.count ("threads"))
				{
					threads = parsed["threads"].as<unsigned> ();
				}
				if (operation == "gtest")
				{
					testing::InitGoogleTest (&argc, argv);
					result = RUN_ALL_TESTS ();
				}
				else if (operation == "dump")
				{
					if (driver != nullptr)
						driver->dump ();
				}
				else if (operation == "profile")
				{
					auto threads_l (threads != 0 ? threads : driver->threads_get ());
					auto driver_difficulty (nano_pow::bit_difficulty (difficulty));
					auto threshold (nano_pow::reverse (driver_difficulty));
					std::cout << "Profiling threads: " << std::to_string (threads_l) << " lookup: " << std::to_string (nano_pow::to_megabytes (nano_pow::entries_to_memory (lookup_entries))) << "MB threshold: " << to_string_hex128 (threshold) << " difficulty: " << to_string_hex128 (driver_difficulty) << " (" << to_string_hex64 (nano_pow::difficulty_128_to_64 (driver_difficulty)) << ")" << std::endl;
					profile (*driver, threads, driver_difficulty, nano_pow::entries_to_memory (lookup_entries), count);
				}
				else if (operation == "profile_validation")
				{
					profile_validate (std::max (10000000U, count));
				}
				else if (operation == "tune")
				{
					auto threshold (nano_pow::reverse (nano_pow::bit_difficulty (difficulty)));
					// Force threads and lookup if not given
					//TODO any device performing better with less than 4096 threads for a reasonable difficulty?
					auto threads_l (threads != 0 ? threads : std::min (static_cast<size_t> (4096), driver->threads_get ()));
					if (parsed.count ("lookup") == 0 && driver->type () == nano_pow::driver_type::OPENCL)
					{
						lookup = 32;
						lookup_entries = nano_pow::lookup_to_entries (lookup);
					}
					std::cout << "Tuning for difficulty " << difficulty << " starting with " << threads_l << " threads and " << nano_pow::to_megabytes (nano_pow::entries_to_memory (lookup_entries)) << "MB memory " << std::endl;
					std::cout << "This may take a while..." << std::endl;
					tune (driver.get (), nano_pow::reverse (threshold), count, threads_l, nano_pow::entries_to_memory (lookup_entries));
				}
				else
				{
					std::cerr << "Invalid operation. Available: {gtest, dump, profile, profile_validation, tune}" << std::endl;
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
		std::cerr << err.what () << "\n\n"
		          << options.help () << std::endl;
	}
	catch (nano_pow::OCLDriverException const & err)
	{
		std::cerr << "OpenCL error" << std::endl;
		err.print (std::cerr);
	}
	return result;
}
