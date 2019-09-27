#include <nano_pow/tuning.hpp>

#include <sstream>

size_t nano_pow::solve_many (nano_pow::driver & driver_a, size_t const count_a)
{
	auto start (std::chrono::steady_clock::now ());
	for (unsigned i{ 0 }; i < count_a; ++i)
	{
		driver_a.solve ({ i + 1, 0 });
	}
	auto duration = (std::chrono::steady_clock::now () - start).count ();
	return duration;
}

bool nano_pow::tune (nano_pow::cpp_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & best_memory_a)
{
	std::ostringstream oss;
	return tune (driver_a, count_a, initial_memory_a, initial_threads_a, best_memory_a, oss);
}

bool nano_pow::tune (nano_pow::opencl_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & max_memory_a, size_t & best_memory_a, size_t & best_threads_a)
{
	std::ostringstream oss;
	return tune (driver_a, count_a, initial_memory_a, initial_threads_a, max_memory_a, best_memory_a, best_threads_a, oss);
}

bool nano_pow::tune (nano_pow::cpp_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & best_memory_a, std::ostream & stream)
{
	auto megabytes = [](auto const memory) { return memory / (1024 * 1024); };

	size_t constexpr min_memory = (1ULL << 18) * 4;
	size_t constexpr max_memory = (1ULL << 32) * 4;
	size_t memory (initial_memory_a);
	driver_a.threads_set (initial_threads_a);

	/*
	 * Find the best memory configuration for this difficulty
	 * Start at the recommended difficulty/2 + 1 lookup size
	 * Go down and then up in memory until a worse configuration is found in both directions
	 */
	size_t best_duration = std::chrono::system_clock::duration::max ().count ();
	driver_a.memory_set (memory);
	best_memory_a = memory;
	best_duration = solve_many (driver_a, count_a);
	auto duration = best_duration;
	stream << driver_a.threads_get () << " threads " << megabytes (memory) << "MB average " << duration * 1e-6 / count_a << "ms" << std::endl;
	// Go down in memory
	while (memory > min_memory)
	{
		memory /= 2;
		driver_a.memory_set (memory);
		duration = solve_many (driver_a, count_a);
		stream << driver_a.threads_get () << " threads " << megabytes (memory) << "MB average " << duration * 1e-6 / count_a << "ms" << std::endl;
		if (duration < best_duration)
		{
			best_duration = duration;
			best_memory_a = memory;
		}
		else
		{
			break;
		}
	}
	// Go up in memory
	memory = initial_memory_a;
	while (memory < max_memory)
	{
		memory *= 2;
		driver_a.memory_set (memory);
		duration = solve_many (driver_a, count_a);
		stream << driver_a.threads_get () << " threads " << megabytes (memory) << "MB average " << duration * 1e-6 / count_a << "ms" << std::endl;
		if (duration < best_duration)
		{
			best_duration = duration;
			best_memory_a = memory;
		}
		else
		{
			break;
		}
	}
	stream << "Found best memory " << megabytes (best_memory_a) << "MB" << std::endl;
	driver_a.memory_set (best_memory_a);
	return false;
}

bool nano_pow::tune (nano_pow::opencl_driver & driver_a, unsigned const count_a, size_t const initial_memory_a, size_t const initial_threads_a, size_t & max_memory_a, size_t & best_memory_a, size_t & best_threads_a, std::ostream & stream)
{
	auto megabytes = [](auto const memory) { return memory / (1024 * 1024); };

	size_t constexpr min_memory = (1ULL << 18) * 4;
	auto max_threads (driver_a.max_threads ());
	stream << "Maximum device threads " << max_threads << std::endl;

	bool ok{ false };

	size_t memory (initial_memory_a);
	size_t threads{ initial_threads_a };
	driver_a.threads_set (threads);

	/*
	 * Find the maximum memory available
	 * Memory is allocated in up to 4 slabs due to contiguous memory allocation limits in some devices
	 * This result can vary depending on current usage of the device
	 */
	while (!ok)
	{
		try
		{
			driver_a.memory_set (memory);
			driver_a.fill ();
			//TODO do all cases fail in fill? If not, uncomment next line or replace with solve()
			// search ();
			ok = true;
		}
		catch (OCLDriverException const & err)
		{
			stream << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			memory /= 2;
			if (memory < min_memory)
			{
				stream << "Reached minimum memory " << megabytes (memory) << "MB" << std::endl;
				break;
			}
		}
	}
	if (ok)
	{
		stream << "Found max memory " << megabytes (memory) << "MB" << std::endl;
		max_memory_a = memory;
	}

	/*
	 * Find the best memory configuration for this difficulty
	 */
	size_t best_duration = std::chrono::system_clock::duration::max ().count ();
	while (ok)
	{
		try
		{
			driver_a.memory_set (memory);
			auto duration = solve_many (driver_a, count_a);
			stream << threads << " threads " << megabytes (memory) << "MB average " << duration * 1e-6 / count_a << "ms" << std::endl;
			if (duration < best_duration)
			{
				best_duration = duration;
				memory /= 2;
			}
			else
			{
				memory *= 2;
				break;
			}
		}
		catch (OCLDriverException const & err)
		{
			stream << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			ok = false;
		}
	}
	if (ok)
	{
		best_memory_a = memory;
		stream << "Found best memory " << megabytes (best_memory_a) << "MB" << std::endl;
		driver_a.memory_set (best_memory_a);
	}

	/*
	 * Find the best number of threads in powers of 2
	 */
	threads *= 2;
	while (ok && threads <= max_threads)
	{
		try
		{
			driver_a.threads_set (threads);
			auto duration = solve_many (driver_a, count_a);
			stream << threads << " threads " << megabytes (memory) << "MB average " << duration * 1e-6 / count_a << "ms" << std::endl;
			if (duration < best_duration)
			{
				best_duration = duration;
				threads *= 2;
			}
			else
			{
				threads /= 2;
				break;
			}
		}
		catch (OCLDriverException const & err)
		{
			stream << threads << " threads, " << megabytes (memory) << "MB FAIL :: " << err.origin () << " :: " << err.what () << std::endl;
			ok = false;
		}
	}
	if (ok)
	{
		best_threads_a = threads;
		stream << "Found best threads " << threads << std::endl;
		driver_a.threads_set (best_threads_a);
	}

	return !ok;
}