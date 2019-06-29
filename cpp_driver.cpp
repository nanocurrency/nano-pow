#include <cpp_driver.hpp>

#include <ssp_pow/pow.hpp>
#include <ssp_pow/hash.hpp>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

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

ssp_pow::cpp_driver::cpp_driver () :
context (hash, { 0, 0 }, nullptr, 0, context.bit_threshold (8))
{
	threads_set (std::thread::hardware_concurrency ());
}

ssp_pow::cpp_driver::~cpp_driver ()
{
	threads_set (0);
	lookup_set (0);
}

void ssp_pow::cpp_driver::barrier (std::unique_lock<std::mutex> & lock)
{
	condition.wait (lock, [this] () { return ready == threads.size (); });
}

uint64_t ssp_pow::cpp_driver::solve (std::array <uint64_t, 2> nonce)
{
	std::unique_lock<std::mutex> lock (mutex);
	barrier (lock);
	generator.ticket = 0;
	next_value = 0;
	enable = true;
	this->context.nonce = nonce;
	this->context.hash.reset (nonce);
	worker_condition.notify_all ();
	condition.wait (lock);
	enable = false;
	return generator.result;
}

void ssp_pow::cpp_driver::dump () const
{
	std::cerr << boost::str (boost::format ("Hardware threads: %1%\n") % std::to_string (std::thread::hardware_concurrency ()));
}

void ssp_pow::cpp_driver::lookup_set(size_t lookup)
{
	assert ((lookup & (lookup - 1)) == 0);
	if (context.slab)
	{
		munmap (context.slab, context.size * sizeof (uint32_t));
	}
	context.size = lookup;
	context.slab = lookup == 0 ? nullptr : reinterpret_cast <uint32_t *> (mmap (0, lookup * sizeof (uint32_t), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NOCACHE, -1, 0));
}

void ssp_pow::cpp_driver::threads_set (unsigned threads)
{
	std::unique_lock<std::mutex> lock (mutex);
	barrier (lock);
	while (this->threads.size () < threads)
	{
		this->threads.push_back (std::make_unique<std::thread> ([this, i=this->threads.size ()] () {
			run_loop (i);
			condition.notify_one ();
		}));
	}
	while (this->threads.size () > threads)
	{
		auto thread (std::move (this->threads.back ()));
		worker_condition.notify_all ();
		lock.unlock ();
		thread->join ();
		lock.lock ();
		this->threads.pop_back ();
	}
}

unsigned ssp_pow::cpp_driver::threads_get () const
{
	return threads.size ();
}

void ssp_pow::cpp_driver::threshold_set (uint64_t threshold)
{
	context.threshold = threshold;
}

void ssp_pow::cpp_driver::run_loop (unsigned thread_id)
{
	std::unique_lock<std::mutex> lock (mutex);
	while (threads [thread_id] != nullptr)
	{
		++ready;
		condition.notify_one ();
		worker_condition.wait (lock);
		--ready;
		if (enable && threads [thread_id] != nullptr)
		{
			auto threads_size (threads.size ());
			lock.unlock ();
			auto found (generator.find (context, 0, thread_id, threads_size));
			lock.lock ();
			if (found)
			{
				condition.notify_one ();
			}
		}
	}
}
