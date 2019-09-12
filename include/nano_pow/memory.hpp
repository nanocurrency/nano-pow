#pragma once

#include <memory>

namespace nano_pow
{
void memory_init ();
void free_page_memory (uint8_t * slab, size_t size);
uint8_t * alloc (size_t memory, bool & error);
}
