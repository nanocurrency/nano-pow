#pragma once

#include <nano_pow/driver.hpp>

#include <nano_pow/pow.hpp>

#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace nano_pow
{
	class cpp_driver : public driver
	{
	public:
		cpp_driver ();
		~cpp_driver ();
		void difficulty_set (uint64_t difficulty) override;
		uint64_t difficulty_get () const override;
		void threads_set (unsigned threads) override;
		size_t threads_get () const override;
		bool memory_set (size_t memory) override;
		bool solve (std::array<uint64_t, 2> nonce, uint64_t & result) override;
		bool ok() const override;
		void dump () const override;
	private:
		void barrier (std::unique_lock<std::mutex> & lock);
		nano_pow::context context;
		nano_pow::generator generator;
		void run_loop (size_t);
		std::atomic<unsigned> ready { 0 };
		bool enable { false };
		std::vector<std::unique_ptr<std::thread>> threads;
		std::mutex mutex;
		std::condition_variable condition;
		std::condition_variable worker_condition;
	};
}
