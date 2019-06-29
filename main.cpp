#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <ssp_pow/pow.hpp>
#include <cpp_driver.hpp>
#include <opencl_driver.hpp>
#include <gtest/gtest.h>

namespace {
enum class driver_type
{
	gtest,
	cpp,
	opencl
};
std::ostream & operator << (std::ostream & os, driver_type const & obj)
{
	switch (obj)
	{
		case driver_type::gtest:
			os << "gtest";
			break;
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
	if (text == "gtest")
	{
		obj = driver_type::gtest;
	}
	else if (text == "cpp")
	{
		obj = driver_type::cpp;
	}
	else if (text == "opencl")
	{
		obj = driver_type::opencl;
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
}

int main (int argc, char **argv)
{
	boost::program_options::options_description description ("Command line options");
	description.add_options ()
	("help", "Print out options")
	("driver", boost::program_options::value<driver_type> ()->default_value (driver_type::gtest), "Specify which test driver to use")
	("difficulty", boost::program_options::value<unsigned> ()->default_value (52), "Solution difficulty 1-64 default: 52")
	("threads", boost::program_options::value<unsigned> (), "Number of device threads to use to find solution")
	("lookup", boost::program_options::value<unsigned> (), "Scale of lookup table (N). Table contains 2^N entries, N defaults to (difficulty/2)")
	("count", boost::program_options::value<unsigned> ()->default_value (16), "Specify how many problems to solve, default 16")
	("platform", boost::program_options::value<std::string> (), "Defines the <platform> for OpenCL driver")
	("device", boost::program_options::value<std::string> (), "Defines <device> for OpenCL driver")
	("dump", "Dumping OpenCL information");
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
		else if (vm.count ("dump"))
		{
			std::cout << "Dumping OpenCL information" << std::endl;
			ssp_pow::opencl_environment environment;
			environment.dump (std::cout);
		}
		else
		{
			std::unique_ptr<ssp_pow::driver> driver;
			auto driver_type_l (vm.find ("driver")->second.as<driver_type> ());
			switch (driver_type_l)
			{
				case driver_type::gtest:
				{
					testing::InitGoogleTest(&argc, argv);
					result = RUN_ALL_TESTS();
					break;
				}
				case driver_type::cpp:
				{
					driver = std::make_unique<ssp_pow::cpp_driver> ();
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
					driver = std::make_unique<ssp_pow::opencl_driver> (platform, device);
					break;
				}
			}
			auto difficulty (vm.find ("difficulty")->second.as<unsigned> ());
			auto lookup (difficulty / 2 + 1);
			auto lookup_opt (vm.find ("lookup"));
			if (lookup_opt != vm.end ())
			{
				lookup = lookup_opt->second.as <unsigned> ();
			}
			if (driver != nullptr)
			{
				driver->threshold_set ((1ULL << difficulty) - 1);
				driver->lookup_set (1ULL << lookup);
				auto threads_opt (vm.find ("threads"));
				if (threads_opt != vm.end ())
				{
					driver->threads_set (threads_opt->second.as <unsigned> ());
				}
				auto count (vm.find ("count")->second.as<unsigned> ());
				uint64_t total_time (0);
				for (auto i (0UL); i < count; ++i)
				{
					auto start (std::chrono::steady_clock::now ());
					std::array <uint64_t, 2> nonce = { i + 1, 0 };
					auto result (driver->solve (nonce));
					auto search_time (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now () - start).count ());
					total_time += search_time;
					auto lhs (result >> 32);
					ssp_pow::blake2_hash hash;
					hash.reset (nonce);
					auto lhs_hash (hash (lhs | ssp_pow::context::lhs_or_mask));
					auto rhs (result & 0xffffffffULL);
					auto rhs_hash (hash (rhs & ssp_pow::context::rhs_and_mask));
					auto sum (lhs_hash + rhs_hash);
					std::cerr << boost::str (boost::format (
															"%1%=H0(%2%)+%3%=H1(%4%)=%5% solution ms: %6%\n")
											 % to_string_hex (lhs_hash)
											 % to_string_hex (lhs)
											 % to_string_hex (rhs_hash)
											 % to_string_hex (rhs)
											 % to_string_hex64 (sum)
											 % std::to_string (search_time));
				}
				std::cerr << boost::str (boost::format ("Average solution time: %1%\n") % std::to_string (total_time / count));
			}
		}
	}
	catch (boost::program_options::error const & err)
	{
		std::cerr << err.what () << std::endl;
	}
	return result;
}
