#include <boost/program_options.hpp>

#include <cpp_driver.hpp>
#include <opencl_driver.hpp>
#include <gtest/gtest.h>

namespace {
enum class driver
{
	gtest,
	cpp,
	opencl
};
std::ostream & operator << (std::ostream & os, driver const & obj)
{
	switch (obj)
	{
		case driver::gtest:
			os << "gtest";
			break;
		case driver::cpp:
			os << "cpp";
			break;
		case driver::opencl:
			os << "opencl";
			break;
	}
	return os;
}
std::istream & operator >> (std::istream & is, driver & obj)
{
	std::string text;
	is >> text;
	if (text == "gtest")
	{
		obj = driver::gtest;
	}
	else if (text == "cpp")
	{
		obj = driver::cpp;
	}
	else if (text == "opencl")
	{
		obj = driver::opencl;
	}
	return is;
}
}

int main (int argc, char **argv)
{
	boost::program_options::options_description description ("Command line options");
	description.add_options ()
	("help", "Print out options")
	("driver", boost::program_options::value<driver> ()->default_value (driver::gtest), "Specify which test driver to use");
	boost::program_options::variables_map vm;
	int result (1);
	try
	{
		boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
		boost::program_options::notify (vm);
		driver driver_l = driver::gtest;
		auto driver_opt (vm.find ("driver"));
		if (driver_opt != vm.end ())
		{
			driver_l = driver_opt->second.as<driver>();
		}
		switch (driver_l)
		{
			case driver::gtest:
			{
				testing::InitGoogleTest(&argc, argv);
				result = RUN_ALL_TESTS();
				break;
			}
			case driver::cpp:
			{
				result = cpp_pow_driver::main (argc, argv);
				break;
			}
			case driver::opencl:
			{
				result = opencl_pow_driver::main (argc, argv);
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
