#pragma once

#include <memory>

#include "./abc.h"

using namespace std;


template<class Allocator = allocator<char>>
class WrapperOfOldStyleAllocator: public Allocator {
private:
    Allocator allocator;

public:
    WrapperOfOldStyleAllocator(Allocator allocator): allocator(allocator){}
    WrapperOfOldStyleAllocator(): allocator(){}

    const Allocator getAllocator() const {
        return this->allocator;
    }

    Allocator getAllocator() {
        return this->allocator;
    }

    void* allocate(size_t size) override {
        size_t sizeWithHint = sizeof(size_t) + size;

        size_t* ptr = (size_t*)((void*) allocator_traits<Allocator>::allocate(this->allocator, sizeWithHint));
        *ptr = size;
        ++ptr;
        return (void*) ptr;
    }

    void* allocate(size_t size, const void* help) override {
        size_t* helpRebound = --((size_t*) help);
        size_t sizeWithHint = sizeof(size_t) + size;

        size_t* ptr = (size_t*)((void*) allocator_traits<Allocator>::allocate(this->allocator, sizeWithHint, (void*) helpRebound));
        *ptr = size;
        ++ptr;
        return (void*) ptr;
    }

    void deallocate(void* ptr) override {
        size_t* ptrRebound = --((size_t*) ptr);
        size_t size = *ptrRebound;
        size_t sizeWithHint = sizeof(size_t) + size;

        allocator_traits<Allocator>::deallocate(this->allocator, (char*) ptrRebound, sizeWithHint);
    }
};


template<class T>
class WrapperOfNewStyleAllocator {
private:
    Allocator* allocator;
public:
    WrapperOfNewStyleAllocator(Allocator* allocator): allocator(allocator) {}
    WrapperOfNewStyleAllocator(Allocator& allocator): allocator(&allocator) {}

    const Allocator* getAllocator() const {
        return this->allocator;
    }
    Allocator* getAllocator() {
        return this->allocator;
    }

    T* allocate(size_t count) override {
        return (T*) this->allocator->allocate(sizeof(T) * count);
    }

    void deallocate(T* ptr, size_t count) override {
        return this->allocator->deallocate((void*) ptr);
    }
};
