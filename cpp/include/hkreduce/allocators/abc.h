#pragma once

#include <cstdint>
#include <cstddef>

using namespace std;

class Allocator {
public:
    virtual void* allocate(size_t size) = 0;
    virtual void* allocate(size_t size, const void* help) {
        return this->allocate(size);
    };
    
    virtual void deallocate(void* ptr) noexcept = 0;

    virtual size_t getMaxSize(size_t elementSize) const {
        if(elementSize == 0){
            return SIZE_MAX;
        }
        return SIZE_MAX / elementSize;
    }

    virtual ~Allocator(){}
};
