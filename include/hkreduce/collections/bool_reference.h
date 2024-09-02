#pragma once

#include <cstddef>

using namespace std;


class BoolReference {
private:
    void* const ptr;
    const size_t bitIdx;

public:
    BoolReference(void* ptr, size_t bitIdx): ptr(ptr), bitIdx(bitIdx) {}

    // to make this class as trivial
    BoolReference() = default;

    operator bool() const {
        size_t charOffset = this->bitIdx / sizeof(unsigned char);
        size_t bitIdx = this->bitIdx / sizeof(unsigned char);
        unsigned char bits = *(((unsigned char*) this->ptr) + charOffset);
        return (bits >> this->bitIdx) & 1;
    }

    BoolReference& operator=(const BoolReference& value) {
        return this->operator=((bool) value);
    }

    BoolReference& operator=(bool value){
        size_t charOffset = this->bitIdx / sizeof(unsigned char);
        size_t bitIdx = this->bitIdx / sizeof(unsigned char);
        unsigned char bits = *(((unsigned char*) this->ptr) + charOffset);
        if(value){
            bits |= 1 << bitIdx;
        }else{
            bits &= ~(1 << bitIdx);
        }
        return *this;
    }

    bool operator==(bool value) const {
        return (bool) *this == value;
    }

    bool operator==(const BoolReference& value) const {
        return this->operator==((bool) value);
    }
};