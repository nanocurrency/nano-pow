#include <cpp_driver.hpp>

#include <ssp_pow/pow.hpp>
#include <ssp_pow/hash.hpp>

#include <boost/format.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <sys/mman.h>

#ifndef MAP_NOCACHE
/* No MAP_NOCACHE on Linux */
#define MAP_NOCACHE (0)
#endif

namespace cpp_pow_driver
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
	class environment
	{
	public:
		environment (size_t items_a) :
		items (items_a),
		slab (reinterpret_cast <uint32_t *> (mmap (0, memory (), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NOCACHE, -1, 0))) { }
		~environment () { munmap (slab, memory ()); }
		size_t memory () const
		{
			return items * sizeof (uint32_t);
		};
		size_t const items { 1024ULL * 1024 };
		uint32_t * const slab;
		std::atomic<uint64_t> next_value { 0 };
	};
	void perf_test ()
	{
		std::cerr << "Initializing...\n";
		environment environment (8ULL * 1024 * 1024);
		memset (environment.slab, 0, environment.memory ());
		std::cerr << "Starting...\n";
		
		std::atomic<unsigned> solution_time (0);
		auto nonce_count (16);
		for (auto j (0UL); j < nonce_count; ++j)
		{
			environment.next_value = 0;
			std::atomic<bool> error (true);
			std::atomic<unsigned> fill_time (0);
			auto stepping (65535);
			auto start (std::chrono::system_clock::now ());
			std::vector<std::thread> threads;
			auto thread_count (4);
			for (auto i (0); i < thread_count; ++i)
			{
				std::array <uint64_t, 2> nonce = { j, 0 };
				ssp_pow::blake2_hash hash (nonce);
				ssp_pow::context<ssp_pow::blake2_hash> context (hash, 48);
				threads.emplace_back ([&, i] ()
				{
					uint64_t last_fill (0xffffffff00000000ULL);
					while (error)
					{
						auto begin (environment.next_value.fetch_add (stepping));
						auto fill (begin & last_fill);
						if (fill != last_fill)
						{
							last_fill = fill;
							auto fill_ratio (1.0 / thread_count);
							context.fill (environment.slab, environment.items, fill_ratio * environment.items, environment.items * i);
							fill_time += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now () - start).count ();
						}
						auto solution (context.search (environment.slab, environment.items, stepping, begin));
						if (solution != 0)
						{
							auto search_time (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now () - start).count ());
							solution_time += search_time;
							auto lhs (solution >> 32);
							auto lhs_hash (hash (lhs | context.lhs_or_mask));
							auto rhs (solution & 0xffffffffULL);
							auto rhs_hash (hash (rhs & context.rhs_and_mask));
							auto sum (lhs_hash + rhs_hash);
							std::cerr << boost::str (boost::format ("%1%=H0(%2%)+%3%=H1(%4%)=%5% solution ms: %6% fill ms %7%\n") % to_string_hex (lhs_hash) % to_string_hex (lhs) % to_string_hex (rhs_hash) % to_string_hex (rhs) % to_string_hex64 (sum) % std::to_string (search_time) % std::to_string (fill_time / thread_count));
							error = false;
						}
					}
				});
			}
			for (auto & i: threads)
			{
				i.join ();
			}
		}
		solution_time = solution_time / nonce_count;
		std::cerr << boost::str (boost::format ("Average solution time: %1%\n") % std::to_string (solution_time));
	}
	void perf_test2 ()
	{
		std::cerr << "Initializing...\n";
		environment environment (8ULL * 1024 * 1024);
		memset (environment.slab, 0, environment.memory ());
		std::cerr << "Starting...\n";
		
		std::atomic<unsigned> solution_time (0);
		auto nonce_count (16);
		for (auto j (0UL); j < nonce_count; ++j)
		{
			auto start (std::chrono::system_clock::now ());
			std::vector<std::thread> threads;
			auto thread_count (4);
			for (auto i (0); i < thread_count; ++i)
			{
				std::array <uint64_t, 2> nonce = { j, 0 };
				ssp_pow::blake2_hash hash (nonce);
				ssp_pow::context<ssp_pow::blake2_hash> context (hash, 48);
				ssp_pow::generator<ssp_pow::blake2_hash> generator (context);
				unsigned ticket (generator.ticket);
				threads.emplace_back ([&, i] ()
				{
					generator.find (environment.slab, environment.items, ticket, i, thread_count);
					if (i == 0)
					{
						auto search_time (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now () - start).count ());
						solution_time += search_time;
						auto lhs (generator.result >> 32);
						auto lhs_hash (hash (lhs | context.lhs_or_mask));
						auto rhs (generator.result & 0xffffffffULL);
						auto rhs_hash (hash (rhs & context.rhs_and_mask));
						auto sum (lhs_hash + rhs_hash);
						std::cerr << boost::str (boost::format ("%1%=H0(%2%)+%3%=H1(%4%)=%5% solution ms: %6%\n") % to_string_hex (lhs_hash) % to_string_hex (lhs) % to_string_hex (rhs_hash) % to_string_hex (rhs) % to_string_hex64 (sum) % std::to_string (search_time));
					}
				});
			}
			for (auto & i: threads)
			{
				i.join ();
			}
		}
		solution_time = solution_time / nonce_count;
		std::cerr << boost::str (boost::format ("Average solution time: %1%\n") % std::to_string (solution_time));
	}
	int main (int argc, char **argv)
	{
		perf_test2 ();
		return 0;
	}
}
