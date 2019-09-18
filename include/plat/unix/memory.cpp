#include <nano_pow/memory.hpp>
#include <nano_pow/pow.hpp>

#include <sys/mman.h>

#ifndef MAP_NOCACHE
/* No MAP_NOCACHE on Linux */
#define MAP_NOCACHE (0)
#endif

namespace nano_pow
{
void memory_init ()
{
	// Empty
}

void free_page_memory (uint8_t * slab, size_t size)
{
	if (slab)
	{
		munmap (slab, size * NP_VALUE_SIZE);
	}
}
uint8_t * alloc (size_t memory, bool & error)
{
	auto alloc = mmap (0, memory, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NOCACHE, -1, 0);
	error |= (alloc == MAP_FAILED);
	return reinterpret_cast<uint8_t *> (alloc);
}
}