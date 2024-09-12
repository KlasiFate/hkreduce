#pragma once

#include <cstdint>

#include "../allocators/abc.h"
#include "../allocators/default.h"
#include "./abc.h"
#include "./sectioned.h"


using namespace std;

class Bitmap: public IndexableCollection<bool> {
public:
    typedef size_t BoolSection;
    static constexpr size_t BITS_COUNT_IN_SECTION = sizeof(BoolSection) * 8;
    static constexpr size_t BOOL_SECTION_SIZE = sizeof(BoolSection);
    static constexpr BoolSection ALL_EQUAL_ONE = SIZE_MAX;
    static constexpr BoolSection ALL_EQUAL_ZERO = 0;

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
        if(this->boolSections->getSize() * BITS_COUNT_IN_SECTION < size){
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
        size_t sectionsCount = size / BITS_COUNT_IN_SECTION;
        if(size % BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        BoolSection sectionValue = ALL_EQUAL_ZERO;
        if(value){
            sectionValue = ALL_EQUAL_ONE;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, sectionValue, allocator, DEFAULT_BLOCK_SIZE / BITS_COUNT_IN_SECTION * 8);
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
        size_t sectionsCount = this->getSize() / BITS_COUNT_IN_SECTION;
        if(this->getSize() % BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, this->getAllocator(), DEFAULT_BLOCK_SIZE / BITS_COUNT_IN_SECTION * 8);
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
        size_t sectionsCount = this->getSize() / BITS_COUNT_IN_SECTION;
        if(this->getSize() % BITS_COUNT_IN_SECTION != 0){
            ++sectionsCount;
        }

        try{
            new (this->boolSections) SectionedCollection<BoolSection>(sectionsCount, this->getAllocator(), DEFAULT_BLOCK_SIZE / BITS_COUNT_IN_SECTION * 8);
        }catch(...){
            this->getAllocator()->deallocate((void*) this->boolSections);
            throw;
        }

        for(size_t i = 0; i < sectionsCount; ++i){
            this->boolSections->append(other.boolSections->operator[](i));
        }
    }

    Bitmap& operator=(Bitmap&& other) noexcept {
        if(&other == this){
            return *this;
        }

        if(boolSections != nullptr && this->deleteBoolSections){
            this->boolSections->~IndexableCollection();
            this->getAllocator()->deallocate((void*) this->boolSections);
        }

        this->boolSections = other.boolSections;
        this->deleteBoolSections = other.deleteBoolSections;
        this->setSize(other.getSize());
        this->setAllocator(other.getAllocator());

        return *this;
    }

    ~Bitmap(){
        if(boolSections != nullptr && this->deleteBoolSections){
            this->boolSections->~IndexableCollection();
            this->getAllocator()->deallocate((void*) this->boolSections);
        }
    }

    size_t getAllocatedSize() const override {
        return this->boolSections->getAllocatedSize() * BITS_COUNT_IN_SECTION;
    }
    
    void resize(size_t size) override {
        if(size < this->getSize()){
            throw invalid_argument("Invalid size argument. It is less than size of the collection");
        }
        size_t sectionsCount = size / BITS_COUNT_IN_SECTION;
        if(size % BITS_COUNT_IN_SECTION != 0){
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
        IndexableCollection<BoolSection>& boolSections = *this->boolSections;
        size_t sectionIdx = idx / BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BITS_COUNT_IN_SECTION;
        return BoolReference((void*) &(boolSections[sectionIdx]), bitIdx);
    }

    const BoolReference operator[](size_t idx) const override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        IndexableCollection<BoolSection>& boolSections = *this->boolSections;
        size_t sectionIdx = idx / BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BITS_COUNT_IN_SECTION;
        return BoolReference((void*) &(boolSections[sectionIdx]), bitIdx);
    }

    bool replace(size_t idx, bool element) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx argument is out of range");
        }
        IndexableCollection<BoolSection>& boolSections = *this->boolSections;
        size_t sectionIdx = idx / 8;
        size_t bitIdx = idx % 8;
        BoolSection& section = boolSections[sectionIdx];
        bool old = (section >> bitIdx) & 1;
        if(element){
            section |= 1 << bitIdx;
        }else{
            section &= ~(1 << bitIdx);
        }
        return old;
    }

    void insert(size_t idx, bool element) override {
        if(idx > this->getSize()){
            throw out_of_range("Idx is out of range");
        }

        IndexableCollection<BoolSection>& boolSections = *this->boolSections;

        if(boolSections.getSize() * BITS_COUNT_IN_SECTION == this->getSize()){
            boolSections.append(BoolSection(0));
        }

        size_t sectionIdx = idx / BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BITS_COUNT_IN_SECTION;
        BoolSection& section = boolSections[sectionIdx];

        bool toMoveNextSection = section >> (BITS_COUNT_IN_SECTION - 1);
        BoolSection bitsBefore = (ALL_EQUAL_ONE >> (BITS_COUNT_IN_SECTION - bitIdx)) & section;
        BoolSection bitsAfter = ((ALL_EQUAL_ONE << bitIdx) & section) << 1;

        section = 0 | bitsBefore | bitsAfter;
        if(element){
            section |= 1 << bitIdx;
        }

        size_t usedSections = this->getSize() / BITS_COUNT_IN_SECTION + 1;
        for(size_t i = sectionIdx + 1; i < usedSections; ++i){
            section = boolSections[i];

            bool tmp = section >> (BITS_COUNT_IN_SECTION - 1); 

            section <<= 1;
            if(toMoveNextSection){
                section |= 1;
            }

            toMoveNextSection = tmp;
        }

        this->setSize(this->getSize() + 1);
    }

    bool remove(size_t idx) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }

        IndexableCollection<BoolSection>& boolSections = *this->boolSections;

        size_t sectionIdx = idx / BITS_COUNT_IN_SECTION;
        size_t bitIdx = idx % BITS_COUNT_IN_SECTION;

        size_t usedSections = this->getSize() / BITS_COUNT_IN_SECTION;
        if(this->getSize() % BITS_COUNT_IN_SECTION != 0){
            ++usedSections;
        }

        bool toMovePreviousSection = 0;
        for(size_t i = usedSections - 1; i > sectionIdx; --i){
            BoolSection& section = boolSections[i];
            bool tmp = section & 1;
            section >>= 1;
            if(toMovePreviousSection){
                section |= ~(ALL_EQUAL_ONE >> 1);
            }
            toMovePreviousSection = tmp;
        }

        BoolSection& section = boolSections[sectionIdx];

        bool old = (section >> bitIdx) & 1;

        BoolSection bitsBefore = (ALL_EQUAL_ONE >> (BITS_COUNT_IN_SECTION - bitIdx)) & section;
        BoolSection bitsAfter = ((ALL_EQUAL_ONE << (bitIdx + 1)) & section) >> 1;
        if(toMovePreviousSection){
            bitsAfter |= ~(ALL_EQUAL_ONE >> 1);
        }

        section = 0 | bitsBefore | bitsAfter;

        this->setSize(this->getSize() - 1);

        size_t freeSpaceInBits = (boolSections.getSize() * BITS_COUNT_IN_SECTION - this->getSize());
        size_t sectionsToRemove = freeSpaceInBits / (BITS_COUNT_IN_SECTION);
        for(size_t i = 0; i < sectionsToRemove; ++i){
            boolSections.remove(boolSections.getSize() - 1);
        }

        return old;
    }

    void clear() override {
        this->setSize(0);
        this->boolSections->clear();
    }
};