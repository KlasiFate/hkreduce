cmake_minimum_required(VERSION 3.28)

project("hkreduce")

# Добавляем папку наших модулей
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules ${CMAKE_MODULE_PATH})


# Пакет Python
find_package(Python 3.10 REQUIRED COMPONENTS NumPy)

if(NOT Python_FOUND)
    message(FATAL_ERROR "Python is not found")
endif()

if(NOT Python_NumPy_FOUND)
    message(FATAL_ERROR "NumPy is not found")
endif()



set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/bin)


add_subdirectory(src)
add_subdirectory(docs)