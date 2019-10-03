#include <nano_pow/c_api.h>
#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/opencl_driver.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace
{
/** Implements the opaque np_context_t C type */
struct context
{
	context ()
	{
		reset_error ();
	}

	struct context_error
	{
		np_error_category_t kind;
		int64_t error_code;
		std::string message;
	} error;

	void reset_error ()
	{
		error.kind = np_err_success;
		error.message = "";
	}

	bool has_error () const
	{
		return error.kind != np_err_success;
	}

	void set_opencl_error (cl_int error_a, std::string const & message_a = "")
	{
		error.kind = np_err_opencl;
		error.error_code = static_cast<int64_t> (error_a);
		error.message = message_a;
	}

	void set_generic_error (int64_t error_a, std::string const & message_a = "")
	{
		error.kind = np_err_generic;
		error.error_code = error_a;
		error.message = message_a;
	}

	void set_generic_error (np_error_code_t error_a)
	{
		error.kind = np_err_generic;
		error.error_code = error_a;

		switch (error_a)
		{
			case np_err_invalid_index:
				error.message = "Index out of bounds";
				break;
			case np_err_driver_invalid:
				error.message = "Invalid driver";
				break;
			case np_err_driver_invalid_type:
				error.message = "Invalid driver type";
				break;
			case np_err_device_invalid:
				error.message = "Invalid device";
				break;
			case np_err_device_not_found:
				error.message = "Device not found";
				break;
			case np_err_device_list_invalid:
				error.message = "Invalid device list";
				break;
			case np_err_thread_invalid_count:
				error.message = "Invalid thread count";
				break;
			case np_err_difficulty_invalid:
				error.message = "Invalid difficulty";
				break;
			default:
				error.message = "Unknown";
		}
	}

	void set_generic_error (std::string const & message_a = "")
	{
		// The error code is per definition of np_error_code (see header comment)
		set_generic_error (std::numeric_limits<int64_t>::max (), message_a);
	}

	bool debug_mode = false;
};

/** Convert between opaque and implementation pointer types */
template <typename TARGET, typename SOURCE>
TARGET convert (SOURCE obj)
{
	static_assert (std::is_pointer<SOURCE>::value, "Expected a source pointer");
	static_assert (std::is_pointer<TARGET>::value, "Expected a target pointer");
	return (reinterpret_cast<TARGET> (obj));
}

/** Reset the error state of the context. This is a no-op if \ctx is null */
void reset_error (np_context_t * ctx)
{
	if (ctx)
	{
		auto context_l (convert<context *> (ctx));
		context_l->reset_error ();
	}
}

/** Implements the opaque np_opencl_device_t C type */
struct opencl_device
{
	uint16_t platform;
	uint16_t device;
	std::string name;
	std::string vendor;
	bool compiler_available;
	uint64_t memory_available_bytes;
	uint64_t maximum_alloc_size_bytes;
};

/** Implements the opaque np_work_t data type */
struct work
{
	std::array<uint64_t, 2> nonce{ 0, 0 };
	std::array<uint64_t, 2> solution{ 0, 0 };
	uint64_t wrapped_difficulty{ 0 };
	uint64_t lookup_table_bytes;
};

} //anonymous namespace

np_context_t * np_context_create ()
{
	auto context_l = new context ();
	return convert<np_context_t *> (context_l);
}

void np_context_destroy (np_context_t * ctx)
{
	delete convert<context *> (ctx);
}

bool np_failed (np_context_t * ctx)
{
	auto context_l = convert<context *> (ctx);
	return context_l->error.kind != np_err_success;
}

int64_t np_error_code (np_context_t * ctx)
{
	auto context_l = convert<context *> (ctx);
	return np_failed (ctx) ? context_l->error.error_code : 0;
}

np_error_category_t np_error_category (np_context_t * ctx)
{
	auto context_l = convert<context *> (ctx);
	return context_l->error.kind;
}

void np_error_string (np_context_t * ctx, char * error_string_buffer, size_t len)
{
	if (len > 0)
	{
		auto context_l = convert<context *> (ctx);
		if (context_l->error.kind == np_err_success)
		{
			strncpy (error_string_buffer, "success", len - 1);
		}
		else if (!context_l->error.message.empty ())
		{
			strncpy (error_string_buffer, context_l->error.message.c_str (), len - 1);
		}
		else
		{
			strncpy (error_string_buffer, "Unknown error", len - 1);
		}
	}
}

void np_error_category_string (np_context_t * ctx, char * error_string_buffer, size_t len)
{
	if (len > 0)
	{
		auto context_l = convert<context *> (ctx);
		if (context_l->error.kind == np_err_success)
		{
			strncpy (error_string_buffer, "success", len - 1);
		}
		else if (context_l->error.kind == np_err_generic)
		{
			strncpy (error_string_buffer, "generic", len - 1);
		}
		else if (context_l->error.kind == np_err_opencl)
		{
			strncpy (error_string_buffer, "opencl", len - 1);
		}
		else
		{
			strncpy (error_string_buffer, "Invalid error state", len - 1);
		}
	}
}

np_work_t * np_work_create (np_context_t * ctx)
{
	reset_error (ctx);
	return convert<np_work_t *> (new work ());
}

void np_work_destroy (np_context_t * ctx, np_work_t * work_a)
{
	reset_error (ctx);
	delete convert<work *> (work_a);
}

void np_work_set_nonce (np_context_t * ctx, np_work_t * work_a, uint64_t nonce_hi, uint64_t nonce_lo)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	work_l->nonce[0] = nonce_hi;
	work_l->nonce[1] = nonce_lo;
}

void np_work_set_solution (np_context_t * ctx, np_work_t * work_a, uint64_t solution_hi, uint64_t solution_lo)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	work_l->solution[0] = solution_hi;
	work_l->solution[1] = solution_lo;
}

void np_work_set_difficulty (np_context_t * ctx, np_work_t * work_a, uint64_t difficulty_a)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	work_l->wrapped_difficulty = difficulty_a;
}

void np_work_set_table_size (np_context_t * ctx, np_work_t * work_a, uint64_t size_bytes_a)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	work_l->lookup_table_bytes = size_bytes_a;
}

void np_work_get_solution (np_context_t * ctx, np_work_t * work_a, uint64_t * solution_hi, uint64_t * solution_lo)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	*solution_lo = work_l->solution[1];
	*solution_hi = work_l->solution[0];
}

void np_work_get_difficulty (np_context_t * ctx, np_work_t * work_a, uint64_t * difficulty_a)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	*difficulty_a = work_l->wrapped_difficulty;
}

void np_solve (np_context_t * ctx, np_driver_t * driver_a, np_work_t * work_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	auto work_l = convert<work *> (work_a);
	assert (driver_l != nullptr);
	assert (work_l != nullptr);

	if (work_l->lookup_table_bytes == 0)
	{
		context_l->set_generic_error (no_err_work_invalid_memory_size);
	}
	else
	{
		driver_l->difficulty_set_64 (work_l->wrapped_difficulty);
		driver_l->memory_set (work_l->lookup_table_bytes);
		work_l->solution = driver_l->solve (work_l->nonce);
		auto difficulty128 (nano_pow::difficulty (work_l->nonce, work_l->solution));
		work_l->wrapped_difficulty = nano_pow::difficulty_128_to_64 (difficulty128);
	}
}

bool np_validate (np_context_t * ctx, np_work_t * work_a)
{
	reset_error (ctx);
	auto work_l = convert<work *> (work_a);
	return nano_pow::passes_64 (work_l->nonce, work_l->solution, work_l->wrapped_difficulty);
}

np_driver_t * np_driver_opencl_create (np_context_t * ctx, uint16_t platform, uint16_t device)
{
	reset_error (ctx);
	void * driver_l = static_cast<nano_pow::driver *> (new nano_pow::opencl_driver (platform, device));
	return convert<np_driver_t *> (driver_l);
}

np_driver_t * np_driver_cpp_create (np_context_t * ctx)
{
	reset_error (ctx);
	void * driver_l = static_cast<nano_pow::driver *> (new nano_pow::cpp_driver ());
	return convert<np_driver_t *> (driver_l);
}

void np_driver_destroy (np_context_t * ctx, np_driver_t * driver)
{
	reset_error (ctx);
	delete convert<nano_pow::driver *> (driver);
}

void np_driver_dump (np_context_t * ctx, np_driver_t * driver_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		driver_l->dump ();
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
}

uint32_t np_driver_recommended_threads (np_context_t * ctx, np_driver_t * driver_a)
{
	uint32_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		if (driver_l->type () == nano_pow::driver_type::CPP)
		{
			result_l = std::thread::hardware_concurrency ();
		}
		else if (driver_l->type () == nano_pow::driver_type::OPENCL)
		{
			result_l = 8192;
		}
		else
		{
			context_l->set_generic_error (np_err_driver_invalid_type);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
	return result_l;
}

uint32_t np_driver_threads_get (np_context_t * ctx, np_driver_t * driver_a)
{
	uint32_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		auto threads_l (driver_l->threads_get ());
		if (threads_l <= std::numeric_limits<uint32_t>::max ())
		{
			result_l = static_cast<uint32_t> (driver_l->threads_get ());
		}
		else
		{
			context_l->set_generic_error (np_err_thread_invalid_count);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
	return result_l;
}

void np_driver_threads_set (np_context_t * ctx, np_driver_t * driver_a, uint32_t threads_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		if (threads_a > 0)
		{
			driver_l->threads_set (threads_a);
		}
		else
		{
			context_l->set_generic_error (np_err_thread_invalid_count);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
}

uint8_t np_driver_recommended_lookup (np_context_t * ctx, np_driver_t * driver_a, uint8_t difficulty_a)
{
	uint8_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		if (difficulty_a > 0 && difficulty_a < 64)
		{
			if (driver_l->type () == nano_pow::driver_type::CPP)
			{
				//TODO validate this recommendation
				result_l = difficulty_a / 2;
			}
			else if (driver_l->type () == nano_pow::driver_type::OPENCL)
			{
				result_l = (difficulty_a / 2) + 1;
			}
		}
		else
		{
			context_l->set_generic_error (np_err_difficulty_invalid);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
	return result_l;
}

uint64_t np_driver_difficulty_get (np_context_t * ctx, np_driver_t * driver_a)
{
	uint64_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		result_l = driver_l->difficulty_get_64 ();
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
	return result_l;
}

void np_driver_difficulty_set (np_context_t * ctx, np_driver_t * driver_a, uint64_t difficulty_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto driver_l (convert<nano_pow::driver *> (driver_a));
	if (driver_l)
	{
		driver_l->difficulty_set_64 (difficulty_a);
	}
	else
	{
		context_l->set_generic_error (np_err_driver_invalid);
	}
}

np_opencl_device_list_t * np_opencl_device_list_get (np_context_t * ctx)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();

	std::vector<opencl_device> * infos_l = new std::vector<opencl_device> ();
	try
	{
		std::vector<cl::Platform> platforms_l;
		(void)cl::Platform::get (&platforms_l);
		for (size_t platform_count (0); platform_count < platforms_l.size (); ++platform_count)
		{
			auto platform_l = platforms_l[platform_count];
			std::vector<cl::Device> devices_l;
			(void)platform_l.getDevices (CL_DEVICE_TYPE_ALL, &devices_l);
			for (size_t device_count (0); device_count < devices_l.size (); ++device_count)
			{
				auto device_l = devices_l[device_count];
				opencl_device info_l;
				info_l.platform = platform_count;
				info_l.device = device_count;
				info_l.name = device_l.getInfo<CL_DEVICE_NAME> ().c_str ();
				info_l.vendor = device_l.getInfo<CL_DEVICE_VENDOR> ().c_str ();
				info_l.compiler_available = device_l.getInfo<CL_DEVICE_COMPILER_AVAILABLE> ();
				info_l.memory_available_bytes = device_l.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE> ();
				info_l.maximum_alloc_size_bytes = device_l.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE> ();
				infos_l->push_back (info_l);
			}
		}
	}
	catch (cl::Error const & err)
	{
		np_opencl_device_list_destroy (ctx, convert<np_opencl_device_list_t *> (infos_l));
		context_l->set_opencl_error (err.err (), err.what ());
	}
	return convert<np_opencl_device_list_t *> (infos_l);
}

size_t np_opencl_device_count (np_context_t * ctx, np_opencl_device_list_t * info_list_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	size_t result_l = 0;
	if (info_list_a)
	{
		auto info_l (convert<std::vector<opencl_device> *> (info_list_a));
		result_l = info_l->size ();
	}
	else
	{
		context_l->set_generic_error (np_err_device_list_invalid);
	}
	return result_l;
}

np_opencl_device_t * np_opencl_device_get_by_index (np_context_t * ctx, np_opencl_device_list_t * info_list_a, uint16_t index_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();

	opencl_device * info = nullptr;
	if (info_list_a)
	{
		auto info_l (convert<std::vector<opencl_device> *> (info_list_a));
		if (index_a < info_l->size ())
		{
			info = &(*info_l)[index_a];
		}
		else
		{
			context_l->set_generic_error (np_err_invalid_index);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_device_list_invalid);
	}
	return convert<np_opencl_device_t *> (info);
}

np_opencl_device_t * np_opencl_device_get_by_platform_device (np_context_t * ctx, np_opencl_device_list_t * info_list_a, uint16_t platform_a, uint16_t device_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();

	opencl_device * info = nullptr;
	if (info_list_a)
	{
		auto info_l (convert<std::vector<opencl_device> *> (info_list_a));

		// clang-format off
		auto existing (std::find_if (info_l->begin (), info_l->end (), [platform_a, device_a](opencl_device & device_l) {
			return device_l.platform == platform_a && device_l.device == device_a;
		}));
		// clang-format on
		if (existing != info_l->end ())
		{
			info = &(*existing);
		}
		else
		{
			context_l->set_generic_error (np_err_device_not_found);
		}
	}
	else
	{
		context_l->set_generic_error (np_err_device_list_invalid);
	}
	return convert<np_opencl_device_t *> (info);
}

void np_opencl_device_list_destroy (np_context_t * ctx, np_opencl_device_list_t * info_list_a)
{
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();

	if (info_list_a)
	{
		auto info_list_l (convert<std::vector<opencl_device> *> (info_list_a));
		delete info_list_l;
	}
}

uint16_t np_opencl_device_platform_id (np_context_t * ctx, np_opencl_device_t * device_a)
{
	uint16_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->platform;
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

uint16_t np_opencl_device_device_id (np_context_t * ctx, np_opencl_device_t * device_a)
{
	uint16_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->device;
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

const char * np_opencl_device_name (np_context_t * ctx, np_opencl_device_t * device_a)
{
	const char * result_l{ nullptr };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->name.c_str ();
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

const char * np_opencl_device_vendor (np_context_t * ctx, np_opencl_device_t * device_a)
{
	const char * result_l{ nullptr };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->vendor.c_str ();
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

bool np_opencl_device_compiler_available (np_context_t * ctx, np_opencl_device_t * device_a)
{
	bool result_l{ false };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->compiler_available;
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

uint64_t np_opencl_device_memory_available (np_context_t * ctx, np_opencl_device_t * device_a)
{
	uint64_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->memory_available_bytes;
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}

uint64_t np_opencl_device_maximum_alloc_size (np_context_t * ctx, np_opencl_device_t * device_a)
{
	uint64_t result_l{ 0 };
	auto context_l (convert<context *> (ctx));
	context_l->reset_error ();
	auto info_l (convert<opencl_device *> (device_a));
	if (info_l)
	{
		result_l = info_l->maximum_alloc_size_bytes;
	}
	else
	{
		context_l->set_generic_error (np_err_device_invalid);
	}
	return result_l;
}
