#pragma once

#include <memory>

namespace nano_pow
{
void memory_init ();
void free_page_memory (uint32_t * slab, size_t size);
uint32_t * alloc (size_t memory, bool & error);
}
