#pragma once

#include <memory>

namespace nano_pow
{
bool memory_available (size_t &);
void memory_init ();
void free_page_memory (uint32_t * slab, size_t size);
uint32_t * alloc (size_t memory, bool & error);
}
