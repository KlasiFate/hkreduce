cmake_minimum_required(VERSION 3.28)

project("hkreduce")

# Добавляем папку наших cmake модулей
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules ${CMAKE_MODULE_PATH})


set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang")


# XXX: Имя переменной зарезервировано
if(NOT DEFINED LIBRARY_OUTPUT_NAME)
    message(FATAL_ERROR "Name of output library is not provided")
endif()


set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/output)

# 3.19 относитя к версии cmake модуля, а не к питону
find_package(Python3 3.10 COMPONENTS Interpreter NumPy REQUIRED)

if(NOT Python3_FOUND)
    message(FATAL_ERROR "Python is not found")
endif()

if(NOT Python3_NumPy_FOUND)
    message(FATAL_ERROR "NumPy is not found")
endif()


file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/hkreduce/*.cpp)

add_library(hkreduce_cpp_interface SHARED ${SRCS})

if(MSVC)
    target_compile_options(hkreduce_cpp_interface PUBLIC /W4 /std:c++11)
else()
    target_compile_options(hkreduce_cpp_interface PUBLIC -Wall -Wextra -Wpedantic -std=c++11)
endif()

target_include_directories(hkreduce_cpp_interface PUBLIC ${PROJECT_SOURCE_DIR}/cpp/include)
target_include_directories(hkreduce_cpp_interface PUBLIC ${Python3_INCLUDE_DIRS})
target_include_directories(hkreduce_cpp_interface PUBLIC ${Python3_NumPy_INCLUDE_DIRS})

target_link_libraries(hkreduce_cpp_interface PUBLIC Python3::NumPy)

add_compile_definitions(${Python3_DEFINITIONS})
add_link_options(${Python3_LINK_OPTIONS})


set_target_properties(
    hkreduce_cpp_interface
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${LIBRARY_OUTPUT_NAME}
        LINKER_LANGUAGE CXX
)

add_subdirectory(docs)