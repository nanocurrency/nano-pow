#pragma once

#include <boost/program_options.hpp>

namespace cpp_pow_driver
{
int main (boost::program_options::variables_map & vm, unsigned difficulty, unsigned lookup);
}
