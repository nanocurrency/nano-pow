#include <nano_pow/driver.hpp>

std::array<uint64_t, 2> nano_pow::driver::solve (std::array<uint64_t, 2> nonce)
{
	(void)nonce;
	std::array<uint64_t, 2> result_l = { 0, 0 };
	while (result_l[1] == 0)
	{
		fill ();
		result_l = search ();
	}
	return result_l;
}