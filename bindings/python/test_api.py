#!/usr/bin/env python3

from bindings import *

OCL_PLATFORM = 0
OCL_DEVICE = 0
OCL_THREADS = 16384

# Buffer for error messages
err_str = get_error_buffer()


def device_info(context, device):
    platform_id = np_opencl_device_platform_id(context, device)
    device_id = np_opencl_device_device_id(context, device)
    name = np_opencl_device_name(context, device).decode("utf-8")
    vendor = np_opencl_device_vendor(context, device).decode("utf-8")
    compiler_available = np_opencl_device_compiler_available(context, device)
    memory_available = np_opencl_device_memory_available(context, device)
    maximum_alloc_size = np_opencl_device_maximum_alloc_size(context, device)

    return platform_id, device_id, name, vendor, compiler_available, memory_available, maximum_alloc_size


def error_str(context):
    np_error_string(context, err_str, 1024)
    return err_str.value.decode("utf-8")


if __name__ == "__main__":

    context = np_context_create()

    device_info_list = np_opencl_device_list_get(context)
    device_count = np_opencl_device_count(context, device_info_list)

    for device_id in range(device_count):

        device = np_opencl_device_get_by_index(context, device_info_list, device_id)

        # Device information
        device = np_opencl_device_get_by_index(context, device_info_list, device_id)
        platform_id, device_id, name, vendor, compiler_available, memory_available, maximum_alloc_size = device_info(context, device)

        print("Platform {}\nDevice {}\n{} {}\nCompiler ? {}\nMemory {} MB\nMax Alloc {} MB\n\n".format(
            platform_id, device_id, vendor, name, compiler_available,
            memory_available / (1024*1024), maximum_alloc_size / (1024*1024)
        ))

    # Device previously saved
    chosen_device = np_opencl_device_get_by_platform_device(context, device_info_list, OCL_PLATFORM, OCL_DEVICE)
    platform_id, device_id, name, vendor, compiler_available, memory_available, maximum_alloc_size = device_info(context, chosen_device)

    print("Using device: {} {}".format(vendor, name))

    # Make sure we get an error if we are out of bounds, in this case with the generic error code
    np_opencl_device_get_by_index(context, device_info_list, 1000)
    assert np_failed(context)
    assert np_error_code(context) == NP_ERR_INVALID_INDEX
    assert np_error_category(context) == NP_ERR_CATEGORY_GENERIC
    assert error_str(context) == "Index out of bounds"

    # Destruct info
    np_opencl_device_list_destroy(context, device_info_list)

    cpp = np_driver_cpp_create(context)
    print("Recommended Cpp threads: {}".format(np_driver_recommended_threads(context, cpp)))

    ocl = np_driver_opencl_create(context, OCL_PLATFORM, OCL_DEVICE)
    print("Recommended OpenCL threads: {}".format(np_driver_recommended_threads(context, ocl)))

    print("Set OpenCL threads to {}".format(OCL_THREADS))
    np_driver_threads_set(context, ocl, OCL_THREADS)
    print("Get OpenCL threads: {}".format(np_driver_threads_get(context, ocl)))

    print("Cpp recommended lookup for difficulty 61: {}".format(np_driver_recommended_lookup(context, cpp, 61)))
    print("OpenCL recommended lookup for difficulty 61: {}".format(np_driver_recommended_lookup(context, ocl, 61)))

    # Call dump polymorphically
    # print(np_driver_dump(context, cpp))
    # print(np_driver_dump(context, ocl))

    # Test work generation and validation

    work = np_work_create(context)
    np_work_set_nonce(context, work, 1, 0)

    # Difficulty 50 in 64-bit representation
    np_work_set_difficulty(context, work, 0xffffc00000000000)

    # 256MB table size
    np_work_set_table_size(context, work, 1024 * 1024 * 256)
    np_solve (context, ocl, work)
    assert np_validate (context, work) == True

    s0 = ctypes.c_uint64()
    s1 = ctypes.c_uint64()
    difficulty = ctypes.c_uint64()
    np_work_get_solution (context, work, ctypes.byref(s0), ctypes.byref(s1))
    np_work_get_difficulty (context, work, ctypes.byref(difficulty))
    print ("Solution: {} + {}, difficulty {}".format(hex(s0.value), hex(s1.value), hex(difficulty.value)))

    np_work_destroy(context, work)

    # Destruct drivers
    np_driver_destroy(context, cpp)
    np_driver_destroy(context, ocl)

    # Test success indicators
    if not np_failed(context):
        assert np_error_category (context) == NP_ERR_CATEGORY_SUCCESS
        assert error_str(context) == "success"

    np_context_destroy(context)
