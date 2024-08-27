# find_program(
#     SPHINX_EXECUTABLE,
#     NAMES sphinx-build
#     DOC "Path to sphinx-build executable"
# )


execute_process(
    COMMAND which 
    "sphinx-build"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE SPHINX_EXECUTABLE 
    RESULT_VARIABLE SPHINX_EXECUTABLE_NOT_FOUND
)
if(SPHINX_EXECUTABLE_NOT_FOUND)
    message(FATAL_ERROR "sphinx-build not found")
endif()


include(FindPackageHandleStandardArgs)

# Handle standard arguments to find_package like REQUIRED and QUIET
find_package_handle_standard_args(Sphinx
    REQUIRED_VARS SPHINX_EXECUTABLE
    FAIL_MESSAGE "Failed to find sphinx-build executable"
)