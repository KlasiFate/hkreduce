#pragma once

#include <cstdint>

#include "../allocators/abc.h"
#include "../allocators/default.h"
#include "./abc.h"
#include "./sectioned.h"


using namespace std;

class Bitmap: public IndexableCollection<bool> {
public:
    class BoolSection{
    public:
        typedef size_t BitsType;
        static constexpr size_t BITS_COUNT_IN_SECTION = sizeof(size_t) * 8;
        static constexpr BitsType ALL_BITS_EQUAL_ONE = SIZE_MAX;
        static constexpr BitsType ALL_BITS_EQUAL_ZERO = 0;

        static constexpr BitsType BIT_1 = 1;
    private:
        BitsType bits;

    public:
        BoolSection(bool value): bits(value ? ALL_BITS_EQUAL_ONE : ALL_BITS_EQUAL_ZERO){}
        BoolSection() = default;

        BoolSection(const BoolSection&) = default;
        BoolSection& operator=(const BoolSection&) = default;

        bool getBit(size_t bitIdx) const {
            return this->bits & (BIT_1 << bitIdx);
        }

        bool setBit(size_t bitIdx, bool value){
            bool old = this->getBit(bitIdx);
            if(value){
                this->bits |= BIT_1 << bitIdx;
            }else{
                this->bits &= ~(BIT_1 << bitIdx);
            }
            return old;
        }

        bool insert(size_t bitIdx, bool value){
            bool leaved = this->getBit(BITS_COUNT_IN_SECTION - 1);
            BitsType bitsBefore = this->bits & (ALL_BITS_EQUAL_ONE >> (BITS_COUNT_IN_SECTION - bitIdx));
            BitsType bitsAfter = ((ALL_BITS_EQUAL_ONE << bitIdx) & this->bits) << 1;
            this->bits = bitsBefore | bitsAfter;
            this->setBit(bitIdx, value);
            return leaved;
        }

        bool remove(size_t bitIdx, bool leftestValue){
            bool leaved = this->getBit(bitIdx);
            BitsType bitsBefore = this->bits & (ALL_BITS_EQUAL_ONE >> (BITS_COUNT_IN_SECTION - bitIdx));
            BitsType bitsAfter = ((ALL_BITS_EQUAL_ONE << (bitIdx + 1)) & this->bits) >> 1;
            this->bits = bitsBefore | bitsAfter;
            this->setBit(BITS_COUNT_IN_SECTION - 1, leftestValue);
            return leaved;
        }

        size_t countBits() const {
            size_t res = 0;
            for(int i = 0; i < 64; ++i){
                if(this->bits & (BIT_1 << i)){
                    ++res;
                }
            }
            return res;
        }
    
        BoolReference getReference(size_t idx){
            return BoolReference(
                &(this->bits),
                BITS_COUNT_IN_SECTION,
                idx
            );
        }
    };

    static_assert(sizeof(BoolSection) == sizeof(size_t), "BoolSection size doesn't equal sizeof(size_t)");
    static_assert(is_trivial<BoolSection>::value, "BoolSection is not trivial");

private:
    IndexableCollection<BoolSection>* boolSections;
    bool deleteBoolSections;

public:
    Bitmap(
        IndexableCollection<BoolSection>* boolSections,
        size_t size = 0,
        bool deleteBoolSections = true,
        Allocator* allocator = getDefaultAllocator()
    ): IndexableCollection(size, allocator), boolSections(boolSections), deleteBoolSections(deleteBoolSections) {
        if(this->boolSections->getSize() * BoolSection::BITS_COUNT_IN_SECTION < size){
            throw invalid_argument("Count of bits in sections is less than provided size");
        }
        if(this->getAllocator() != boolSections->getAllocator()){
            throw invalid_argument("Allocators of bool sections and their collection is not equals");
        }
    }

    Bitmap(
        size_t size,
        bool value = false,
        Allocator* allocator = getDefaultAllocator()
    ): IndexableCollection(size, allocator), boolSections(nullptr), deleteBoolSections(true){
        this->boolSections = (SectionedCollection<BoolSection>*) this->getAllocator()->allocate(sizeof(SectionedCollection<BoolSection>));
        size_t sectionsCount = size / BoolSection::BITS_COUNT_IN_SECTION;
        if(size % BoolSection::BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, BoolSection(value), allocator, DEFAULT_BLOCK_SIZE / BoolSection::BITS_COUNT_IN_SECTION * 8);
        }catch(...){
            allocator->deallocate((void*) this->boolSections);
            throw;
        }
    }

    Bitmap(): IndexableCollection(0, nullptr), boolSections(nullptr), deleteBoolSections(false) {}

    Bitmap(const Bitmap& other): IndexableCollection(other.getSize(), other.getAllocator()), boolSections(nullptr), deleteBoolSections(true) {
        if(other.boolSections == nullptr){
            this->deleteBoolSections = false;
            return;
        }

        this->boolSections = (SectionedCollection<BoolSection>*) this->getAllocator()->allocate(sizeof(SectionedCollection<BoolSection>));
        size_t sectionsCount = this->getSize() / BoolSection::BITS_COUNT_IN_SECTION;
        if(this->getSize() % BoolSection::BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, this->getAllocator(), DEFAULT_BLOCK_SIZE / BoolSection::BITS_COUNT_IN_SECTION * 8);
        }catch(...){
            this->getAllocator()->deallocate((void*) this->boolSections);
            throw;
        }

        for(size_t i = 0; i < sectionsCount; ++i){
            this->boolSections->append(other.boolSections->operator[](i));
        }
    }

    Bitmap(Bitmap&& other) noexcept: IndexableCollection(other.getSize(), other.getAllocator()), boolSections(other.boolSections), deleteBoolSections(other.deleteBoolSections) {
        other.setSize(0);
        other.setAllocator(nullptr);
        other.deleteBoolSections = false;
        other.boolSections = nullptr;
    }

    Bitmap& operator=(const Bitmap& other){
        if(&other == this){
            return *this;
        }
        
        if(boolSections != nullptr && this->deleteBoolSections){
            this->boolSections->~IndexableCollection();
            this->getAllocator()->deallocate((void*) this->boolSections);
        }

        this->setSize(other.getSize());
        this->setAllocator(other.getAllocator());
        this->deleteBoolSections = true;

        if(other.boolSections == nullptr){
            this->boolSections = nullptr;
            this->deleteBoolSections = false;
            return *this;
        }

        this->boolSections = (SectionedCollection<BoolSection>*) this->getAllocator()->allocate(sizeof(SectionedCollection<BoolSection>));
        size_t sectionsCount = this->getSize() / BoolSection::BITS_COUNT_IN_SECTION;
        if(this->getSize() % BoolSection::BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, this->getAllocator(), DEFAULT_BLOCK_SIZE / BoolSection::ALL_BITS_EQUAL_ONE * 8);
        }catch(...){
            this->getAllocator()->deallocate((void*) this->boolSections);
            throw;
        }

        for(size_t i = 0; i < sectionsCount; ++i){
            this->boolSections->append(other.boolSections->operator[](i));
        }

        return *this;
    }

    Bitmap& operator=(Bitmap&& other) noexcept {
        if(&other == this){
            return *this;
        }

        if(this->boolSections != nullptr && this->deleteBoolSections){
            this->boolSections->~IndexableCollection();
            this->getAllocator()->deallocate((void*) this->boolSections);
        }

        this->boolSections = other.boolSections;
        this->deleteBoolSections = other.deleteBoolSections;
        this->setSize(other.getSize());
        this->setAllocator(other.getAllocator());

        other.boolSections = nullptr;
        other.deleteBoolSections = false;
        other.setSize(0);
        other.setAllocator(nullptr);

        return *this;
    }

    ~Bitmap(){
        if(boolSections != nullptr && this->deleteBoolSections){
            this->boolSections->~IndexableCollection();
            this->getAllocator()->deallocate((void*) this->boolSections);
        }
    }

    size_t getAllocatedSize() const override {
        return this->boolSections->getAllocatedSize() * BoolSection::BITS_COUNT_IN_SECTION;
    }
    
    void resize(size_t size) override {
        if(size < this->getSize()){
            throw invalid_argument("Invalid size argument. It is less than size of the collection");
        }
        size_t sectionsCount = size / BoolSection::BITS_COUNT_IN_SECTION;
        if(size % BoolSection::BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }
        this->boolSections->resize(sectionsCount);
    };
    
    bool getDeleteBoolSections() const {
        return this->deleteBoolSections;
    }
    
    void setDeleteBoolSections(bool deleteBoolSections) {
        this->deleteBoolSections = deleteBoolSections;
    }

    const IndexableCollection<BoolSection>* getBoolSections() const {
        return this->boolSections;
    }
    IndexableCollection<BoolSection>* getBoolSections(){
        return this->boolSections;
    }

    BoolReference operator[](size_t idx) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / BoolSection::BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BoolSection::BITS_COUNT_IN_SECTION;
        return (*this->boolSections)[sectionIdx].getReference(bitIdx);
    }

    const BoolReference operator[](size_t idx) const override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / BoolSection::BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BoolSection::BITS_COUNT_IN_SECTION;
        return (*this->boolSections)[sectionIdx].getReference(bitIdx);
    }

    bool replace(size_t idx, bool element) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx argument is out of range");
        }
        size_t sectionIdx = idx / BoolSection::BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BoolSection::BITS_COUNT_IN_SECTION;
        return (*this->boolSections)[sectionIdx].setBit(bitIdx, element);
    }

    void insert(size_t idx, bool element) override {
        if(idx > this->getSize()){
            throw out_of_range("Idx is out of range");
        }

        IndexableCollection<BoolSection>& boolSections = *this->boolSections;

        if(boolSections.getSize() * BoolSection::BITS_COUNT_IN_SECTION == this->getSize()){
            boolSections.append(BoolSection(0));
        }

        size_t sectionIdx = idx / BoolSection::BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BoolSection::BITS_COUNT_IN_SECTION;
        
        bool leaved = boolSections[sectionIdx].insert(bitIdx, element);
        size_t usedSections = this->getSize() / BoolSection::BITS_COUNT_IN_SECTION + 1;
        for(size_t i = sectionIdx + 1; i < usedSections; ++i){
            leaved = boolSections[i].insert(0, leaved);
        }
        this->setSize(this->getSize() + 1);
    }

    bool remove(size_t idx) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }

        IndexableCollection<BoolSection>& boolSections = *this->boolSections;

        size_t sectionIdx = idx / BoolSection::BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BoolSection::BITS_COUNT_IN_SECTION;

        size_t usedSections = this->getSize() / BoolSection::BITS_COUNT_IN_SECTION;
        if(this->getSize() % BoolSection::BITS_COUNT_IN_SECTION != 0){
            ++usedSections;
        }

        // doesn't matter, but false is best value
        bool leaved = false;
        for(size_t i = usedSections - 1; i > sectionIdx; --i){
            boolSections[i].remove(0, leaved);
        }
        leaved = boolSections[sectionIdx].remove(bitIdx, leaved);
        return leaved;

        this->setSize(this->getSize() - 1);
        if (this->getSize() % BoolSection::BITS_COUNT_IN_SECTION == 0){
            size_t unusedSectionsCount = boolSections.getSize() - (this->getSize() / BoolSection::BITS_COUNT_IN_SECTION);
            for(size_t i = 0; i < unusedSectionsCount; ++i){
                boolSections.remove(boolSections.getSize() - 1);
            }
        }
        return leaved;
    }

    void clear() override {
        this->boolSections->clear();
        this->setSize(0);
    }
};