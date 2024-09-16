#pragma once

#include <cstddef>

using namespace std;


class BoolReference {
private:
    void* ptr;
    size_t sectionSizeInBits;
    size_t bitIdx;

public:
    typedef unsigned char ByteType;
    typedef size_t MultiByteType;

    static constexpr ByteType BYTE_BIT_1 = 1;
    static constexpr MultiByteType MULTI_BYTE_BIT_1 = 1;

    static constexpr size_t BYTE_SIZE_IN_BITS = sizeof(ByteType) * 8;
    static constexpr size_t MULTI_BYTE_SIZE_IN_BITS = sizeof(MultiByteType) * 8;

    BoolReference(void* ptr, size_t sectionSizeInBits, size_t bitIdx): ptr(ptr), sectionSizeInBits(sectionSizeInBits), bitIdx(bitIdx){
        if(bitIdx >= sectionSizeInBits){
            throw invalid_argument("Argument bitIdx is greater than sectionSize");
        }
        if(sectionSizeInBits % 8 != 0){
            throw invalid_argument("Section size in bits which is not multiple of 8 is not supported");
        }
    }

    BoolReference() = default;

    // to make this class trivial
    BoolReference(const BoolReference& other) = default;
    BoolReference& operator=(const BoolReference& other) = default;

    operator bool() const {
        if(this->ptr == nullptr){
            throw invalid_argument("BoolReference is not initialized");
        }
        if(this->sectionSizeInBits < MULTI_BYTE_SIZE_IN_BITS || this->sectionSizeInBits > MULTI_BYTE_SIZE_IN_BITS){
            size_t byteOffset = this->bitIdx / BYTE_SIZE_IN_BITS;
            size_t bitIdx = this->bitIdx % BYTE_SIZE_IN_BITS;
            ByteType bits = *(((ByteType*) this->ptr) + byteOffset);
            return bits & (BYTE_BIT_1 << bitIdx);
        }
        size_t mbyteOffset = this->bitIdx / MULTI_BYTE_SIZE_IN_BITS;
        size_t bitIdx = this->bitIdx % MULTI_BYTE_SIZE_IN_BITS;
        MultiByteType bits = *(((MultiByteType*) this->ptr) + mbyteOffset);
        return bits & (MULTI_BYTE_BIT_1 << bitIdx);
    }

    BoolReference& operator=(bool value){
        if(this->ptr == nullptr){
            throw invalid_argument("BoolReference is not initialized");
        }
        if(this->sectionSizeInBits < MULTI_BYTE_SIZE_IN_BITS || this->sectionSizeInBits > MULTI_BYTE_SIZE_IN_BITS){
            size_t byteOffset = this->bitIdx / BYTE_SIZE_IN_BITS;
            size_t bitIdx = this->bitIdx % BYTE_SIZE_IN_BITS;
            ByteType& bits = *(((ByteType*) this->ptr) + byteOffset);
            if(value){
                bits |= BYTE_BIT_1 << bitIdx;
            }else{
                bits &= ~(BYTE_BIT_1 << bitIdx);
            }
            return *this;
        }
        size_t mbyteOffset = this->bitIdx / MULTI_BYTE_SIZE_IN_BITS;
        size_t bitIdx = this->bitIdx % MULTI_BYTE_SIZE_IN_BITS;
        MultiByteType& bits = *(((MultiByteType*) this->ptr) + mbyteOffset);
        if(value){
            bits |= MULTI_BYTE_BIT_1 << bitIdx;
        }else{
            bits &= ~(MULTI_BYTE_BIT_1 << bitIdx);
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