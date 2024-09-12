cmake_minimum_required(VERSION 3.28)

project("hkreduce")

# Добавляем папку наших cmake модулей
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules ${CMAKE_MODULE_PATH})


if(NOT DEFINED PYTHON_INCLUDE_DIR)
    message(FATAL_ERROR "Python headers dir is not provided")
endif()

if(NOT DEFINED NUMPY_INCLUDE_DIR)
    message(FATAL_ERROR "NumPy headers dir is not provided")
endif()

# XXX: Имя переменной зарезервировано
if(NOT DEFINED LIBRARY_OUTPUT_NAME)
    message(FATAL_ERROR "Name of output library is not provided")
endif()


set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/output)


file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/hkreduce/*.cpp)

add_library(hkreduce_cpp_interface SHARED ${SRCS})

target_include_directories(hkreduce_cpp_interface PUBLIC ${PROJECT_SOURCE_DIR}/cpp/include)
target_include_directories(hkreduce_cpp_interface PUBLIC ${PYTHON_INCLUDE_DIR})
target_include_directories(hkreduce_cpp_interface PUBLIC ${NUMPY_INCLUDE_DIR})

set_target_properties(
    hkreduce_cpp_interface
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${LIBRARY_OUTPUT_NAME}
        LINKER_LANGUAGE CXX
)

add_subdirectory(docs)