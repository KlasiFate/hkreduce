cmake_minimum_required(VERSION 3.28)

project("hkreduce")

# Для документации
add_subdirectory("docs")


# 

# Папка include'ов
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Папка исходников
set(SRC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Папка куда будут сложены скомпилированные файлы, документация и тд
set(OUTPUT_DIR ./output)

find_package(PythonInterp 3.10 REQUIRED)
find_package(PythonLibs 3.10 REQUIRED)

exec_program(
    python
    ARGS "-c \"import numpy; print(numpy.get_include())\""
    OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
    RETURN_VALUE NUMPY_NOT_FOUND
)
if(NUMPY_NOT_FOUND)
    message(FATAL_ERROR "NumPy headers not found")
endif()

add_library(hkreduce, SHARED, ${SRC_DIR})

set_target_properties(
    hkreduce,
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "hkreduce"
        LINKER_LANGUAGE C++
)