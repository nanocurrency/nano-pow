#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <nano_pow/pow.hpp>
#include <cpp_driver.hpp>
#include <opencl_driver.hpp>
#include <gtest/gtest.h>

namespace {
enum class driver_type
{
	cpp,
	opencl
};
std::ostream & operator << (std::ostream & os, driver_type const & obj)
{
	switch (obj)
	{
		case driver_type::cpp:
			os << "cpp";
			break;
		case driver_type::opencl:
			os << "opencl";
			break;
	}
	return os;
}
std::istream & operator >> (std::istream & is, driver_type & obj)
{
	std::string text;
	is >> text;
	if (text == "cpp")
	{
		obj = driver_type::cpp;
	}
	else if (text == "opencl")
	{
		obj = driver_type::opencl;
	}
	return is;
}
enum class operation_type
{
	profile,
	dump,
	gtest
};
std::ostream & operator << (std::ostream & os, operation_type const & obj)
{
	switch (obj)
	{
		case operation_type::profile:
			os << "profile";
			break;
		case operation_type::dump:
			os << "dump";
			break;
		case operation_type::gtest:
			os << "gtest";
			break;
	}
	return os;
}
std::istream & operator >> (std::istream & is, operation_type & obj)
{
	std::string text;
	is >> text;
	if (text == "gtest")
	{
		obj = operation_type::gtest;
	}
	else if (text == "profile")
	{
		obj = operation_type::profile;
	}
	else if (text == "dump")
	{
		obj = operation_type::dump;
	}
	return is;
}
}

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
std::string to_string_solution (nano_pow::context & context_a, uint64_t threshold_a, uint64_t solution_a)
{
	auto lhs (solution_a >> 32);
	auto lhs_hash (context_a.H0 (lhs));
	auto rhs (solution_a & 0xffffffffULL);
	auto rhs_hash (context_a.H1 (rhs));
	auto sum (lhs_hash + rhs_hash);
	return boost::str (boost::format ("H0(%1%)+H1(%2%)=%3% %4%") % to_string_hex (lhs) % to_string_hex (rhs) % to_string_hex64 (sum) % to_string_hex64 (context_a.difficulty (context_a, solution_a)));
}
float profile (nano_pow::driver & driver_a, unsigned threads, uint64_t threshold, uint64_t lookup, unsigned count)
{
	if (threads != 0)
	{
		driver_a.threads_set (threads);
	}
	driver_a.threshold_set (threshold);
	driver_a.lookup_set (lookup);
	uint64_t total_time (0);
	for (auto i (0UL); i < count; ++i)
	{
		auto start (std::chrono::steady_clock::now ());
		std::array <uint64_t, 2> nonce = { i + 1, 0 };
		auto result (driver_a.solve (nonce));
		auto search_time (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now () - start).count ());
		total_time += search_time;
		nano_pow::context context (nonce, nullptr, 0, driver_a.threshold_get ());
		std::cerr << boost::str (boost::format ("%1% solution ms: %2%\n") % to_string_solution (context, driver_a.threshold_get (), result) % std::to_string (search_time));
	}
	std::cerr << boost::str (boost::format ("Average solution time: %1%\n") % std::to_string (total_time / count));
	return total_time / count;
}
}

int main (int argc, char **argv)
{
	boost::program_options::options_description description ("Command line options");
	description.add_options ()
	("help", "Print out options")
	("driver", boost::program_options::value<driver_type> ()->default_value (driver_type::cpp), "Specify which test driver to use")
	("difficulty", boost::program_options::value<unsigned> ()->default_value (52), "Solution difficulty 1-64 default: 52")
	("threads", boost::program_options::value<unsigned> (), "Number of device threads to use to find solution")
	("lookup", boost::program_options::value<unsigned> (), "Scale of lookup table (N). Table contains 2^N entries, N defaults to (difficulty/2)")
	("count", boost::program_options::value<unsigned> ()->default_value (16), "Specify how many problems to solve, default 16")
	("operation", boost::program_options::value<operation_type> ()->default_value(operation_type::gtest), "Specify which driver operration to perform")
	("platform", boost::program_options::value<std::string> (), "Defines the <platform> for OpenCL driver")
	("device", boost::program_options::value<std::string> (), "Defines <device> for OpenCL driver");
	boost::program_options::variables_map vm;
	int result (1);
	try
	{
		boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
		boost::program_options::notify (vm);
		if (vm.count ("help"))
		{
			std::cout << description << std::endl;
		}
		else
		{
			std::unique_ptr<nano_pow::driver> driver;
			auto driver_type_l (vm.find ("driver")->second.as<driver_type> ());
			switch (driver_type_l)
			{
				case driver_type::cpp:
				{
					driver = std::make_unique<nano_pow::cpp_driver> ();
					break;
				}
				case driver_type::opencl:
				{
					unsigned short platform (0);
					auto platform_it = vm.find ("platform");
					if (platform_it != vm.end ())
					{
						try
						{
							platform = boost::lexical_cast<unsigned short> (platform_it->second.as<std::string> ());
						}
						catch (boost::bad_lexical_cast &)
						{
							std::cerr << "Invalid platform id\n";
							result = -1;
						}
					}
					unsigned short device (0);
					auto device_it = vm.find ("device");
					if (device_it != vm.end ())
					{
						try
						{
							device = boost::lexical_cast<unsigned short> (device_it->second.as<std::string> ());
						}
						catch (boost::bad_lexical_cast &)
						{
							std::cerr << "Invalid device id\n";
							result = -1;
						}
					}
					driver = std::make_unique<nano_pow::opencl_driver> (platform, device);
					break;
				}
			}
			std::cerr << boost::str (boost::format ("Driver: %1%\n")  % driver_type_l);
			auto difficulty (vm.find ("difficulty")->second.as<unsigned> ());
			auto lookup (difficulty / 2 + 1);
			auto lookup_opt (vm.find ("lookup"));
			if (lookup_opt != vm.end ())
			{
				lookup = lookup_opt->second.as <unsigned> ();
			}
			auto count (vm.find ("count")->second.as<unsigned> ());
			unsigned threads (0);
			auto threads_opt (vm.find ("threads"));
			if (threads_opt != vm.end ())
			{
				threads = threads_opt->second.as <unsigned> ();
			}
			auto operation_type_l (vm.find ("operation")->second.as<operation_type> ());
			switch (operation_type_l)
			{
				case operation_type::gtest:
					testing::InitGoogleTest(&argc, argv);
					result = RUN_ALL_TESTS();
					break;
				case operation_type::dump:
					if (driver != nullptr)
						driver->dump ();
					break;
				case operation_type::profile:
					if (driver != nullptr)
					{
						std::cerr << boost::str (boost::format ("Profiling threads: %1% lookup: %2%kb threshold: %3%\n") % std::to_string (driver->threads_get ()) % std::to_string ((1ULL << lookup) / 1024) % to_string_hex64 ((1ULL << difficulty) - 1));
						profile (*driver, threads, (1ULL << difficulty) - 1, 1ULL << lookup, count);
					}
					break;
			}
		}
	}
	catch (boost::program_options::error const & err)
	{
		std::cerr << err.what () << std::endl;
	}
	return result;
}
