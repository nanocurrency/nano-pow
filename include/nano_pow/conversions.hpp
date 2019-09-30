#include <cstddef>
#include <cstdint>

#pragma once

namespace nano_pow
{
constexpr size_t entry_size{ sizeof (uint32_t) };

inline size_t to_megabytes (size_t const memory_a)
{
	constexpr size_t megabytes_div{ 1024 * 1024 };
	return memory_a / (megabytes_div);
}

inline size_t entries_to_memory (size_t const entries_a)
{
	return entries_a * (entry_size);
}

inline size_t memory_to_entries (size_t const memory_a)
{
	return memory_a / (entry_size);
}

inline size_t lookup_to_entries (size_t const lookup_a)
{
	return 1ULL << lookup_a;
}
}