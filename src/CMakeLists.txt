file(GLOB_RECURSE SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(hkreduce SHARED ${SRCS})

target_include_directories(hkreduce PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(hkreduce PUBLIC ${Python_INCLUDE_DIRS})
target_include_directories(hkreduce PUBLIC ${Python_NumPy_INCLUDE_DIRS})

set_target_properties(
    hkreduce
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "hkreduce"
        LINKER_LANGUAGE CXX
)