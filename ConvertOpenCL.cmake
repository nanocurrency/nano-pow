file(READ ${CL_FILE} opencl_program)
file(WRITE ${CPP_FILE}
"/* Automatically generated from opencl_program.cl */\n"
"#include <string>\n"
"namespace nano_pow\n"
"{\n"
"std::string opencl_program = R\"OCL(\n"
"${opencl_program}\n"
")OCL\";\n"
"}")