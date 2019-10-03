#ifndef NANO_POW_C_API_H
#define NANO_POW_C_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Error categories */
typedef enum
{
	np_err_success = 0,
	np_err_generic,
	np_err_opencl
} np_error_category_t;

/**
 * Generic error codes. These are based at np_err_base to not collide with system specific error codes.
 * Please refer to the OpenCL documentation for information about OpenCL specific error codes.
 * NOTE: New versions of the API may only deprecate error codes and add new ones to the end of the enum.
 */
typedef enum
{
	np_err_base = 0x80000000,
	np_err_invalid_index,
	np_err_driver_invalid,
	np_err_driver_invalid_type,
	np_err_device_invalid,
	np_err_device_not_found,
	np_err_device_list_invalid,
	np_err_thread_invalid_count,
	np_err_difficulty_invalid,
	no_err_work_invalid_memory_size,
} np_error_code_t;

/**
 * A context maintains the error state and any internal state needed by the C API for a
 * particular client. All API functions resets the error state. Multithreaded clients
 * must create a separate context for each thread.
 */
struct np_context;
typedef struct np_context np_context_t;

struct np_driver;
typedef struct np_driver np_driver_t;

struct np_opencl_device;
typedef struct np_opencl_device np_opencl_device_t;

struct np_opencl_device_list;
typedef struct np_opencl_device_list np_opencl_device_list_t;

struct np_work;
typedef struct np_work np_work_t;

/** Create a new nanopow context */
np_context_t * np_context_create ();

/** Deallocate a nanopow context previously created with np_context_create()  */
void np_context_destroy (np_context_t * ctx);

/** Returns true if the last API call in the given \p ctx resulted in an error. */
bool np_failed (np_context_t * ctx);

/**
 * Returns the last error code associated with the context, or 0 if there is no error. If the source of the error
 * does not report a specific error code, the maximum value of int64_t will be returned.
 * If np_error_category() returns np_err_generic, the error code will be an np_error_code_t value.
 */
int64_t np_error_code (np_context_t * ctx);

/** Returns the category of the last error */
np_error_category_t np_error_category (np_context_t * ctx);

/** Copy the current error message string into the preallocated buffer of the given max length. The string is zero terminated. */
void np_error_string (np_context_t * ctx, char * error_string_buffer, size_t len);

/** Copy the current error category string into the preallocated buffer of the given max length. The string is zero terminated. */
void np_error_category_string (np_context_t * ctx, char * error_string_buffer, size_t len);

/**
 * Create an empty work object which can be populated using np_work setters. The object can then be
 * passed to np_validate()
 */
np_work_t * np_work_create (np_context_t * ctx);

/** Destroy work object previously created by np_work_create() */
void np_work_destroy (np_context_t * ctx, np_work_t * work);

void np_work_set_nonce (np_context_t * ctx, np_work_t * work, uint64_t nonce_hi, uint64_t nonce_lo);
void np_work_set_solution (np_context_t * ctx, np_work_t * work, uint64_t solution_hi, uint64_t solution_lo);
void np_work_set_table_size (np_context_t * ctx, np_work_t * work, uint64_t size_bytes);

void np_work_set_difficulty (np_context_t * ctx, np_work_t * work, uint64_t value);
void np_work_get_solution (np_context_t * ctx, np_work_t * work, uint64_t * solution_hi, uint64_t * solution_lo);
void np_work_get_difficulty (np_context_t * ctx, np_work_t * work, uint64_t * difficulty);

/**
 * Generate proof-of-work given a 128-bit nonce and 64-bit difficulty.
 * The work object will be updated with a 128-bit solution which can be read with np_work_set_solution()
 * @param driver The driver to use for work generation
 * @param work nonce and difficulty must be set. On completion, this object will be updated with the solution.
 */
void np_solve (np_context_t * ctx, np_driver_t * driver, np_work_t * work);

/**
 * Validate proof-of-work given a 128-bit nonce, 128-bit solution, and 64-bit difficulty
 * The difficulty of the work object is updated, which can be read with np_work_get_difficulty()
 * @param work Nonce, solution and difficulty *must* be set, otherwise error is set to \b no_err_work_invalid_input
 * @return true if the work proof is valid
 */
bool np_validate (np_context_t * ctx, np_work_t * work);

// Driver
np_driver_t * np_driver_opencl_create (np_context_t * ctx, uint16_t platform, uint16_t device);
np_driver_t * np_driver_cpp_create (np_context_t * ctx);
void np_driver_destroy (np_context_t * ctx, np_driver_t * driver);
void np_driver_dump (np_context_t * ctx, np_driver_t * driver);

// Threads
uint32_t np_driver_recommended_threads (np_context_t * ctx, np_driver_t * driver);
uint32_t np_driver_threads_get (np_context_t * ctx, np_driver_t * driver);
void np_driver_threads_set (np_context_t * ctx, np_driver_t * driver, uint32_t threads);

// Driver lookup and difficulty
uint8_t np_driver_recommended_lookup (np_context_t * ctx, np_driver_t * driver, uint8_t difficulty);
uint64_t np_driver_difficulty_get (np_context_t * ctx, np_driver_t * driver);
void np_driver_difficulty_set (np_context_t * ctx, np_driver_t * driver, uint64_t difficulty);

// OpenCL device information
np_opencl_device_list_t * np_opencl_device_list_get (np_context_t * ctx);
size_t np_opencl_device_count (np_context_t * ctx, np_opencl_device_list_t * infos);
np_opencl_device_t * np_opencl_device_get_by_index (np_context_t * ctx, np_opencl_device_list_t * infos, uint16_t index);
np_opencl_device_t * np_opencl_device_get_by_platform_device (np_context_t * ctx, np_opencl_device_list_t * infos, uint16_t platform, uint16_t device);
void np_opencl_device_list_destroy (np_context_t * ctx, np_opencl_device_list_t * infos);
uint16_t np_opencl_device_platform_id (np_context_t * ctx, np_opencl_device_t * info);
uint16_t np_opencl_device_device_id (np_context_t * ctx, np_opencl_device_t * info);
const char * np_opencl_device_name (np_context_t * ctx, np_opencl_device_t * info);
const char * np_opencl_device_vendor (np_context_t * ctx, np_opencl_device_t * info);
bool np_opencl_device_compiler_available (np_context_t * ctx, np_opencl_device_t * info);
uint64_t np_opencl_device_memory_available (np_context_t * ctx, np_opencl_device_t * info);
uint64_t np_opencl_device_maximum_alloc_size (np_context_t * ctx, np_opencl_device_t * info);

#ifdef __cplusplus
}
#endif

#endif
