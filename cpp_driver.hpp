#pragma once

#include <nano_pow/driver.hpp>

#include <nano_pow/pow.hpp>

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace nano_pow
{
	class cpp_driver : public driver
	{
	public:
		cpp_driver ();
		~cpp_driver ();
		void threshold_set (uint64_t threshold) override;
		uint64_t threshold_get () const override;
		void threads_set (unsigned threads) override;
		unsigned threads_get () const override;
		void lookup_set (size_t lookup) override;
		uint64_t solve (std::array<uint64_t, 2> nonce) override;
		void dump () const override;
	private:
		void barrier (std::unique_lock<std::mutex> & lock);
		nano_pow::context context;
		nano_pow::generator generator;
		void run_loop (unsigned);
		std::atomic<unsigned> ready { 0 };
		bool enable { false };
		std::vector<std::unique_ptr<std::thread>> threads;
		std::mutex mutex;
		std::condition_variable condition;
		std::condition_variable worker_condition;
	};
}
