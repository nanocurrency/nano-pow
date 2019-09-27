#include <nano_pow/memory.hpp>

#include <cassert>

#define NOMINMAX
#include <windows.h>

namespace
{
size_t round_up (size_t num_to_round, size_t multiple)
{
	if (multiple == 0)
	{
		return num_to_round;
	}

	auto remainder = num_to_round % multiple;
	if (remainder == 0)
	{
		return num_to_round;
	}

	return num_to_round + multiple - remainder;
}

size_t use_large_mem_pages{ false };
}

namespace nano_pow
{
bool memory_available (size_t & memory)
{
	bool error{ false };
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof (statex);
	if (GlobalMemoryStatusEx (&statex))
	{
		memory = static_cast<size_t> (statex.ullAvailPhys);
	}
	else
	{
		error = true;
	}
	return error;
}

void memory_init ()
{
	HANDLE hToken = nullptr;
	if (OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		TOKEN_PRIVILEGES tp;
		if (LookupPrivilegeValue (NULL, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid))
		{
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			if (AdjustTokenPrivileges (hToken, FALSE, &tp, 0, nullptr, nullptr))
			{
				use_large_mem_pages = (GetLargePageMinimum () != 0);
			}
		}
	}

	if (hToken)
	{
		CloseHandle (hToken);
	}
}

uint32_t * alloc (size_t memory, bool & error)
{
	auto extra_flags = 0u;
	auto rounded_up_memory = memory;
	if (use_large_mem_pages)
	{
		// Memory must be a multiple of GetLargePageMinimum ()
		rounded_up_memory = round_up (memory, GetLargePageMinimum ());
		extra_flags = MEM_LARGE_PAGES;
	}

	auto alloc = VirtualAlloc (nullptr, rounded_up_memory, MEM_COMMIT | MEM_RESERVE | extra_flags, PAGE_READWRITE);
	if (!alloc && use_large_mem_pages)
	{
		// There was an issue using the large memory pages locked in physical memory, so try and allocate without.
		alloc = VirtualAlloc (nullptr, memory, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		error |= (alloc == nullptr);
	}
	return reinterpret_cast<uint32_t *> (alloc);
}

void free_page_memory (uint32_t * slab, size_t)
{
	if (slab)
	{
		auto success = VirtualFree (slab, 0, MEM_RELEASE);
		assert (success);
	}
}
}