file(READ ${CL_FILE} opencl_program)
file(WRITE ${CPP_FILE} "#include <string>\n"
"#ifndef NP_VALUE_SIZE\n"
"#define NP_VALUE_SIZE 5\n"
"#endif\n"
"namespace nano_pow\n"
"{\n"
"std::string opencl_program = R\"OCL(\n"
"${opencl_program}\n"
")OCL\";\n"
"}")