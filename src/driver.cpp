#include <nano_pow/driver.hpp>

std::array<uint64_t, 2> nano_pow::driver::solve (std::array<uint64_t, 2> nonce)
{
	cancel = false;
	(void)nonce;
	std::array<uint64_t, 2> result_l = { 0, 0 };
	while (!cancel && result_l[1] == 0)
	{
		fill ();
		if (!cancel)
		{
			result_l = search ();
		}
	}
	return result_l;
}