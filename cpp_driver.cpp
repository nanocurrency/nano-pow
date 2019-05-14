#include <cpp_driver.hpp>

#include <ssp_pow/pow.hpp>

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

namespace cpp_pow_driver
{
	std::string to_string_hex (uint64_t value_a)
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
		ssp_pow::context context (48);
		std::cerr << "Initializing...\n";
		environment environment (8ULL * 1024 * 1024);
		memset (environment.slab, 0, environment.memory ());
		std::cerr << "Starting...\n";
		
		uint64_t nonce[2] = { 0, 0 };
		std::atomic<unsigned> solution_time (0);
		auto nonce_count (16);
		for (; nonce [0] < nonce_count; ++nonce[0])
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
				threads.emplace_back ([&, i] ()
				{
					auto fill_ratio (1.0 / thread_count);
					context.fill (environment.slab, environment.items, nonce, fill_ratio * environment.items, environment.items * i);
					fill_time += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now () - start).count ();
					while (error)
					{
						auto begin (environment.next_value.fetch_add (stepping));
						auto solution (context.search (environment.slab, environment.items, nonce, stepping, begin));
						if (solution[0] != 0 && solution[1] != 0)
						{
							auto search_time (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now () - start).count ());
							solution_time += search_time;
							std::cerr << boost::str (boost::format ("Solution: %1%,%2% solution ms: %3% fill ms %4%\n") % to_string_hex (solution [0]) % to_string_hex (solution [1]) % std::to_string (search_time) % std::to_string (fill_time / thread_count));
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
	
int main (int argc, char **argv)
{
	perf_test ();
	return 0;
}
}
