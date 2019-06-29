#pragma once

#include <ssp_pow/driver.hpp>
#include <ssp_pow/hash.hpp>

#include <ssp_pow/pow.hpp>

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <array>
#include <thread>

namespace ssp_pow
{
	class cpp_driver : public driver
	{
	public:
		cpp_driver ();
		~cpp_driver ();
		void threshold_set (uint64_t threshold) override;
		void threads_set (unsigned threads) override;
		void lookup_set (size_t lookup) override;
		virtual uint64_t solve (std::array<uint64_t, 2> nonce) override;
	private:
		void barrier (std::unique_lock<std::mutex> & lock);
		ssp_pow::blake2_hash hash;
		ssp_pow::context context;
		ssp_pow::generator generator;
		void run_loop (unsigned);
		std::atomic<unsigned> ready { 0 };
		std::atomic<uint64_t> next_value { 0 };
		std::vector<std::unique_ptr<std::thread>> threads;
		std::mutex mutex;
		std::condition_variable condition;
		std::condition_variable worker_condition;
	};
}
