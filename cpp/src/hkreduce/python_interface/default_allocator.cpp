#include <cstddef>
#include <stdexcept>

#include "Python.h"

#include "hkreduce/allocators/abc.h"
#include "hkreduce/allocators/default.h"

using namespace std;

class PyBadAlloc : public bad_alloc {
private:
    const char* msg;
public:
    PyBadAlloc(const char* msg) : msg(msg) { }

    const char* what() const noexcept override {
        return this->msg;
    }
};

class WrapperOfPyAllocator : public Allocator {
public:
    WrapperOfPyAllocator(){}

    void* allocate(size_t size) override {
        void* ptr = PyMem_RawMalloc(size);
        if (ptr == NULL) {
            throw PyBadAlloc("PyMem_RawMalloc returned NULL");
        }
        return ptr;
    }

    void deallocate(void* ptr) noexcept override {
        return PyMem_RawFree(ptr);
    }
};

Allocator* getDefaultAllocator(){
    static WrapperOfPyAllocator allocator;
    return &allocator;
}