cmake_minimum_required(VERSION 3.28)

project("hkreduce")

# Добавляем папку наших cmake модулей
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules ${CMAKE_MODULE_PATH})


set(CMAKE_C_COMPILER "g++")
set(CMAKE_CXX_COMPILER "g++")


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

# XXX: Имя переменной зарезервировано
if(NOT DEFINED LIBRARY_OUTPUT_NAME)
    message(WRANING "Name of output library is not provided")
    set(LIBRARY_OUTPUT_NAME "cpp_interface")
endif()

if(MSVC)
    target_compile_options(hkreduce_cpp_interface PUBLIC /W4)
else()
    target_compile_options(hkreduce_cpp_interface PUBLIC -Wall -Wextra -Wpedantic -lstdc++)
endif()
target_compile_features(hkreduce_cpp_interface PUBLIC cxx_std_11)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    if(MSVC)
        target_compile_options(hkreduce_cpp_interface PUBLIC /fsanitize=address /fsanitize=memory /fsanitize=null /fsanitize-address-use-after-scope /fno-omit-frame-pointer /fno-optimize-sibling-calls /O0)
        target_link_options(hkreduce_cpp_interface PUBLIC /fsanitize=address /fsanitize=null /fsanitize-address-use-after-scope /fsanitize=leak)
    else()
        target_compile_options(hkreduce_cpp_interface PUBLIC -fsanitize=address -fsanitize=null -fsanitize-address-use-after-scope -fno-omit-frame-pointer -fno-optimize-sibling-calls -O0)
        target_link_options(hkreduce_cpp_interface PUBLIC -fsanitize=address -fsanitize=null -fsanitize-address-use-after-scope -fsanitize=leak)
    endif()
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