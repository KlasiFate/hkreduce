#pragma once

#include <stdexcept>

#include "Python.h"

#include "graph_reducing/allocators/abc.h"

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
    void* allocate(size_t size) override {
        void* ptr = PyMem_RawMalloc(size);
        if (ptr == NULL) {
            throw PyBadAlloc("PyMem_RawMalloc returns NULL");
        }
    }

    void deallocate(void* ptr) noexcept override {
        return PyMem_RawFree(ptr);
    }
};

WrapperOfPyAllocator* getWrapperOfPyAllocator();