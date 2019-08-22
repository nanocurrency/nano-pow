#include <nano_pow/cpp_driver.hpp>

#include <nano_pow/pow.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <plat/win/mman.h>
#else
#include <sys/mman.h>
#endif

#ifndef MAP_NOCACHE
/* No MAP_NOCACHE on Linux */
#define MAP_NOCACHE (0)
#endif

nano_pow::cpp_driver::cpp_driver () :
context ({ 0, 0 }, nullptr, 0, nullptr, context.bit_difficulty (8))
{
	threads_set (std::thread::hardware_concurrency ());
}

nano_pow::cpp_driver::~cpp_driver ()
{
	threads_set (0);
	memory_set (0);
}

void nano_pow::cpp_driver::barrier (std::unique_lock<std::mutex> & lock)
{
	condition.wait (lock, [this] () { return ready == threads.size (); });
}

void nano_pow::cpp_driver::solve (std::array <uint64_t, 2> nonce, uint64_t & result)
{
	std::unique_lock<std::mutex> lock (mutex);
	barrier (lock);
	generator.ticket = 0;
	generator.current = 0;
	enable = true;
	this->context.nonce [0] = nonce [0];
	this->context.nonce [1] = nonce [1];
	worker_condition.notify_all ();
	condition.wait (lock);
	enable = false;
	result = generator.result;
}

void nano_pow::cpp_driver::dump () const
{
	std::cerr << "Hardware threads: " << std::to_string (std::thread::hardware_concurrency ()) << std::endl;
}

bool nano_pow::cpp_driver::memory_set (size_t memory)
{
	assert ((memory & (memory - 1)) == 0);
	if (context.slab)
	{
		munmap (context.slab, context.size * sizeof (uint32_t));
	}
	context.size = memory / sizeof (uint32_t);
	context.slab = memory == 0 ? nullptr : reinterpret_cast <uint32_t *> (mmap (0, memory, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NOCACHE, -1, 0));
	bool error (context.slab == MAP_FAILED);
	if (error)
	{
		std::cerr << "Error while creating memory buffer: MAP_FAILED" << std::endl;
	}
	return error;
}

void nano_pow::cpp_driver::threads_set (unsigned threads)
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

size_t nano_pow::cpp_driver::threads_get () const
{
	return threads.size ();
}

void nano_pow::cpp_driver::difficulty_set (uint64_t difficulty_a)
{
	context.difficulty_inv = nano_pow::context::reverse (difficulty_a);
	context.difficulty_m = difficulty_a;
}

uint64_t nano_pow::cpp_driver::difficulty_get () const
{
	return context.difficulty_m;
}

void nano_pow::cpp_driver::run_loop (size_t thread_id)
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
