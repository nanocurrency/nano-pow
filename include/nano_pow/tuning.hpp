#pragma once

#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/driver.hpp>
#include <nano_pow/opencl_driver.hpp>

namespace nano_pow
{
size_t solve_many (nano_pow::driver & driver_a, size_t const count_a);

bool tune (cpp_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & best_memory_a);
bool tune (cpp_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & best_memory_a, std::ostream & stream);

bool tune (opencl_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & max_memory_a, size_t & best_memory_a, size_t & best_threads_a);
bool tune (opencl_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & max_memory_a, size_t & best_memory_a, size_t & best_threads_a, std::ostream & stream);
}
