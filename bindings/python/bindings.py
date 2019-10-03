import ctypes
import platform

# get the right filename
if platform.uname()[0] == "Windows":
    libname = "libnano_pow.dll"
if platform.uname()[0] == "Linux":
    libname = "libnano_pow.so"
else:
    libname = "libnano_pow.dylib"

def get_error_buffer():
    return ctypes.create_string_buffer(1024)

# The library must be on the path/symlinked
nano_pow = ctypes.cdll.LoadLibrary(libname)

NP_ERR_CATEGORY_SUCCESS = 0
NP_ERR_CATEGORY_GENERIC = NP_ERR_CATEGORY_SUCCESS + 1
NP_ERR_CATEGORY_OPENCL = NP_ERR_CATEGORY_SUCCESS + 2

NP_ERR_BASE = 0x80000000
NP_ERR_INVALID_INDEX = NP_ERR_BASE + 1
NP_ERR_DRIVER_INVALID = NP_ERR_BASE + 2
NP_ERR_DRIVER_INVALID_TYPE = NP_ERR_BASE + 3
NP_ERR_DEVICE_INVALID = NP_ERR_BASE + 4
NP_ERR_DEVICE_NOT_FOUND = NP_ERR_BASE + 5
NP_ERR_DEVICE_LIST_INVALID = NP_ERR_BASE + 6
NP_ERR_THREAD_INVALID_COUNT = NP_ERR_BASE + 7
NP_ERR_DIFFICULTY_INVALID = NP_ERR_BASE + 8

np_context_create = nano_pow.np_context_create
np_context_create.restype = ctypes.c_void_p

np_context_destroy = nano_pow.np_context_destroy
np_context_destroy.argtypes = [ctypes.c_void_p]

np_failed = nano_pow.np_failed
np_failed.argtypes = [ctypes.c_void_p]
np_failed.restype = ctypes.c_bool

np_error_string = nano_pow.np_error_string
np_error_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]

np_error_code = nano_pow.np_error_code
np_error_code.restype = ctypes.c_int64
np_error_code.argtypes = [ctypes.c_void_p]

np_error_category = nano_pow.np_error_category
np_error_category.restype = ctypes.c_uint
np_error_category.argtypes = [ctypes.c_void_p]

np_work_create = nano_pow.np_work_create
np_work_create.argtypes = [ctypes.c_void_p]
np_work_create.restype = ctypes.c_void_p

np_work_destroy = nano_pow.np_work_destroy
np_work_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

np_work_set_nonce = nano_pow.np_work_set_nonce
np_work_set_nonce.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint64];

np_work_set_solution = nano_pow.np_work_set_solution
np_work_set_solution.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint64];

np_work_set_difficulty = nano_pow.np_work_set_difficulty
np_work_set_difficulty.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64];

np_work_set_table_size = nano_pow.np_work_set_table_size
np_work_set_table_size.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64];

np_work_get_solution = nano_pow.np_work_get_solution
np_work_get_solution.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)];

np_work_get_difficulty = nano_pow.np_work_get_difficulty
np_work_get_difficulty.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64)];

np_solve = nano_pow.np_solve
np_solve.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

np_validate = nano_pow.np_validate
np_validate.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

np_driver_cpp_create = nano_pow.np_driver_cpp_create
np_driver_cpp_create.argtypes = [ctypes.c_void_p]
np_driver_cpp_create.restype = ctypes.c_void_p

np_driver_opencl_create = nano_pow.np_driver_opencl_create
np_driver_opencl_create.argtypes = [
    ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
np_driver_opencl_create.restype = ctypes.c_void_p

np_driver_destroy = nano_pow.np_driver_destroy
np_driver_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

np_driver_dump = nano_pow.np_driver_dump
np_driver_dump.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

np_driver_recommended_threads = nano_pow.np_driver_recommended_threads
np_driver_recommended_threads.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_driver_recommended_threads.restype = ctypes.c_uint32

np_driver_threads_get = nano_pow.np_driver_threads_get
np_driver_threads_get.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_driver_threads_get.restype = ctypes.c_uint32

np_driver_threads_set = nano_pow.np_driver_threads_set
np_driver_threads_set.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]

np_driver_recommended_lookup = nano_pow.np_driver_recommended_lookup
np_driver_recommended_lookup.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint8]
np_driver_recommended_lookup.restype = ctypes.c_uint8

np_opencl_device_list_get = nano_pow.np_opencl_device_list_get
np_opencl_device_list_get.argtypes = [ctypes.c_void_p]
np_opencl_device_list_get.restype = ctypes.c_void_p

np_opencl_device_count = nano_pow.np_opencl_device_count
np_opencl_device_count.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_count.restype = ctypes.c_uint16

np_opencl_device_list_destroy = nano_pow.np_opencl_device_list_destroy
np_opencl_device_list_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

np_opencl_device_get_by_index = nano_pow.np_opencl_device_get_by_index
np_opencl_device_get_by_index.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint16]
np_opencl_device_get_by_index.restype = ctypes.c_void_p

np_opencl_device_get_by_platform_device = nano_pow.np_opencl_device_get_by_platform_device
np_opencl_device_get_by_platform_device.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
np_opencl_device_get_by_platform_device.restype = ctypes.c_void_p

np_opencl_device_platform_id = nano_pow.np_opencl_device_platform_id
np_opencl_device_platform_id.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_platform_id.restype = ctypes.c_uint16

np_opencl_device_device_id = nano_pow.np_opencl_device_device_id
np_opencl_device_device_id.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_device_id.restype = ctypes.c_uint16

np_opencl_device_name = nano_pow.np_opencl_device_name
np_opencl_device_name.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_name.restype = ctypes.c_char_p

np_opencl_device_vendor = nano_pow.np_opencl_device_vendor
np_opencl_device_vendor.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_vendor.restype = ctypes.c_char_p

np_opencl_device_compiler_available = nano_pow.np_opencl_device_compiler_available
np_opencl_device_compiler_available.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_compiler_available.restype = ctypes.c_bool

np_opencl_device_memory_available = nano_pow.np_opencl_device_memory_available
np_opencl_device_memory_available.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_memory_available.restype = ctypes.c_uint64

np_opencl_device_maximum_alloc_size = nano_pow.np_opencl_device_maximum_alloc_size
np_opencl_device_maximum_alloc_size.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
np_opencl_device_maximum_alloc_size.restype = ctypes.c_uint64
