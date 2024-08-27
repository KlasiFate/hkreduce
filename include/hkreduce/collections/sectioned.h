#pragma once

#include "../allocators/default.h"
#include "./abc.h"
#include "./array_based.h"
#include "./constants.h"


template<class T>
class SectionedCollection : public IndexableCollection<T> {
private:
    class Section: public ArrayCollection<T> {
    public:
        T* getPtr(size_t idx){
            return this->array + idx;
        }

        void constructAt(size_t idx, const T& value){
            new (this->array + idx) T(value);
            this->setSize(++this->getSize());
            ++this->initalizedElements;
        }

        Section(size_t sectionSize, Allocator* allocator): ArrayCollection((T*) allocator->allocate(sizeof(T) * sectionSize), sectionSize, 0, true, allocator) {}
    };

    typedef DArrayCollection<Section> Sections;

    Sections sections;
    size_t sectionSize;
    bool collectionInitialized;

    void allocateSections(size_t allocated, Allocator* allocator, size_t sectionSize){
        size_t sectionsCount = allocated / sectionSize + (allocated % sectionSize == 0 ? 0 : 1);
        size_t allocatedSizeOfArray = (sectionsCount / 1024 + (sectionsCount % 1024 == 0 ? 0 : 1)) * 1024;
        
        this->sections = Sections(
            (Section*) allocator->allocate(sizeof(Section) * allocatedSizeOfArray),
            allocatedSizeOfArray,
            0,
            true,
            allocator,
            1024
        );
        size_t wasAllocated = 0;
        for (; wasAllocated < sectionsCount; ++wasAllocated) {
            this->sections.append(Section(sectionSize, allocator));
        }
    }

public:
    SectionedCollection() :
        IndexableCollection(0),
        sections(),
        sectionSize(0),
        collectionInitialized(false) { }

    SectionedCollection(
        size_t allocated,
        Allocator* allocator = getDefaultAllocator(),
        size_t sectionSize = DEFAULT_BLOCK_SIZE
    ) :
        IndexableCollection(0, allocator),
        sections(),
        sectionSize(sectionSize),
        collectionInitialized(true) {
        if (sectionSize == 0) {
            throw new invalid_argument("Section size equals zero");
        }
        if (allocator == nullptr) {
            throw new invalid_argument("Allocator can't equal nullptr")
        }
        this->allocateSections(allocated, allocator, sectionSize);
    }

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(
        size_t size,
        const T& value,
        Allocator* allocator = getDefaultAllocator(),
        size_t sectionSize = DEFAULT_BLOCK_SIZE
    ) : SectionedCollection(size, allocator, sectionSize) {
        for (size_t i = 0; i < size; ++i) {
            size_t sectionIdx = i / sectionSize;
            size_t idxInSection = i % sectionSize;
            this->sections[sectionIdx].constructAt(idxInSection, value);
        }
        this->setSize(size);
    };

    template<enable_if_t<!is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(const SectionedCollection<T>& other) = delete;

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(const SectionedCollection<T>& other) :
        IndexableCollection(other.getSize(), other.getAllocator()),
        sections(),
        sectionSize(other.sectionSize),
        collectionInitialized(other.collectionInitialized) {
        if (!other.collectionInitialized) {
            return;
        }

        this->allocateSections(other.getSize(), other.getAllocator(), other.sectionSize);

        for (size_t i = 0; i < size; ++i) {
            size_t sectionIdx = i / other.sectionSize;
            size_t idxInSection = i % other.sectionSize;
            this->sections[sectionIdx].constructAt(idxInSection, other.sections[sectionIdx][idxInSection]);
        }
    }

    SectionedCollection(SectionedCollection<T>&& other) noexcept:
        IndexableCollection(other.getSize(), other.getAllocator()),
        sections(move(other.sections)),
        sectionSize(other.sectionSize),
        collectionInitialized(other.collectionInitialized) { };

    template<enable_if_t<!is_copy_constructible<T>::value, bool> = true>
    SectionedCollection& operator=(const SectionedCollection<T>& other) = delete;

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection& operator=(const SectionedCollection<T>& other) {
        if (this == &other) {
            return;
        }

        if(other.collectionInitialized){
            this->allocateSections(other.getSize(), other.getAllocator(), other.sectionSize);
            
            for (size_t i = 0; i < size; ++i) {
                size_t sectionIdx = i / other.sectionSize;
                size_t idxInSection = i % other.sectionSize;
                this->sections[sectionIdx].constructAt(idxInSection, other.sections[sectionIdx][idxInSection]);
            }

            this->setSize(other.getSize());
            this->setAllocator(other.getAllocator());
            this->sectionSize = other.sectionSize;
            this->collectionInitialized = true;
            return;
        }

        this->setSize(other.getSize());
        this->setAllocator(other.getAllocator());
        this->sections = other.sections;
        this->sectionSize = other.sectionSize;
        this->collectionInitialized = other.collectionInitialized;
    }

    SectionedCollection& operator=(SectionedCollection<T>&& other) noexcept {
        if (this == &other) {
            return;
        }

        this->sections = move(other.sections);
        this->sectionSize = other.sectionSize;
        this->collectionInitialized = other.collectionInitialized;
        this->setSize(other.getSize());
        this->setAllocator(other.getAllocator());
    }

    size_t getAllocatedSize() const {
        return this->sectionSize * this->sections->getSize();
    };

    size_t getSectionSize() const {
        return this->sectionSize;
    }

    T& operator[](size_t idx) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / sectionSize;
        size_t idxInSection = idx % sectionSize;
        return this->sections[sectionIdx][idxInSection];
    };
    const T& operator[](size_t idx) const override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / this->sectionSize;
        size_t idxInSection = idx % this->sectionSize;
        return this->sections[sectionIdx][idxInSection];
    };

    template<enable_if_t<is_copy_assignable<T>::value&& is_move_constructible<T>::value, bool> = true>
    T replace(size_t idx, const T& element) override {
        if (idx > this->getSize()) {
            throw out_of_range("idx argument is out of range");
        }
        size_t sectionIdx = idx / this->getSectionSize();
        size_t idxInSection = idx % this->getSectionSize();
        T old(move(this->sections[sectionIdx][idxInSection]));
        try{
            this->sections[sectionIdx][idxInSection] = element;
        }catch(...){
            this->sections[sectionIdx][idxInSection] = move(old);
            throw;
        }
        return old;
    };

    T replace(size_t idx, T&& element) override {
        if (idx > this->getSize()) {
            throw out_of_range("idx argument is out of range");
        }
        size_t sectionIdx = idx / this->getSectionSize();
        size_t idxInSection = idx % this->getSectionSize();
        T old(move(this->sections[sectionIdx][idxInSection]));
        this->sections[sectionIdx][idxInSection] = element;
        return old;
    };

    void resize(size_t newSpaceSize) override {
        if(newSpaceSize < this->getSize()){
            throw invalid_argument("New space size is less than current size");
        }

        size_t newSectionCount = newSpaceSize / this->sectionSize;
        if(newSpaceSize % this->sectionSize != 0){
            ++newSectionCount;
        }

        if(newSectionCount > this->sections.getSize()){
            for(size_t i = 0; i < newSectionCount - this->sections.getSize(); ++i){
                this->sections.append(Section(this->sectionSize, this->getAllocator()));
            }
        }else{
            for(size_t i = 0; i < this->sections.getSize() - newSectionCount; ++i){
                this->sections.remove(this->sections.getSize() - 1);
            }
        }
    }

    template<enable_if_t<is_copy_assignable<T>::value&& is_copy_constructible<T>::value, bool> = true>
    void insert(size_t idx, const T& element) {
        if (idx > this->getSize()) {
            throw out_of_range("idx argument is out of range");
        }
        if (this->getSize() == this->sectionSize * this->sections.getSize()) {
            this->resize(this->getSize() + this->sectionSize);
        }

        size_t sectionIdx = idx / this->sectionSize;
        size_t idxInSection = idx % this->sectionSize;

        Section& section = this->sections[sectionIdx];
        if(section.getSize() < this->sectionSize){
            section.insert(idxInSection, element);
            this->setSize(++this->getSize());
            return;
        }

        size_t lastUsedSectionIdx = this->getSize() / this->sectionSize;
        for(size_t i = lastUsedSectionIdx; i > sectionIdx; --i){
            Section& prevSection = this->sections[i - 1];
            this->sections[i].insert(0, this->sections[i - 1].remove(this->sectionSize - 1))
        }

        try{
            section.insert(idxInSection, element);
        }catch(...){
            for(size_t i = sectionIdx; i < lastUsedSectionIdx; ++i){
                this->sections[i].append(this->sections[i + 1].remove(0));
            }

            size_t sectionsCountToRemove = (this->getAllocatedSize() - this->getSize()) / this->sectionSize;
            for(size_t i = 0; i < sectionsCountToRemove; ++i){
                this->sections.remove(this->sections.getSize() - 1);
            }
            throw;
        }

        this->setSize(++this->getSIze());
    }

    void insert(size_t idx, T&& element) {
        if (idx > this->getSize()) {
            throw out_of_range("idx argument is out of range");
        }
        if (this->getSize() == this->sectionSize * this->sections.getSize()) {
            this->resize(this->getSize() + this->sectionSize);
        }

        size_t sectionIdx = idx / this->sectionSize;
        size_t idxInSection = idx % this->sectionSize;

        Section& section = this->sections[sectionIdx];
        if(section.getSize() < this->sectionSize){
            section.insert(idxInSection, element);
            this->setSize(++this->getSIze());
            return;
        }

        size_t lastUsedSectionIdx = (this->getSize() + 1) / this->sectionSize;
        for(size_t i = lastUsedSectionIdx; i > sectionIdx; --i){
            Section& prevSection = this->sections[i - 1];
            this->sections[i].insert(0, this->sections[i - 1].remove(this->sectionSize - 1))
        }

        section.insert(idxInSection, element);

        this->setSize(++this->getSIze());
    }
    
    T remove(size_t idx) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / this->sectionSize;
        size_t idxInSection = idx % this->sectionSize;

        Section& section = this->sections[sectionIdx];
        T result(section.remove(sectionIdx));

        if(section.getSize() + 1 == this->sectionSize){
            size_t lastUsedSectionIdx = (this->getSize() - 1) / this->sectionSize;
            for(size_t i = sectionIdx; i < lastUsedSectionIdx; ++i){
                this->sections[i].append(this->sections[i + 1].remove(0));
            }
        }

        this->setSize(--this->getSize());

        size_t sectionsCountToRemove = (this->getAllocatedSize() - this->getSize()) / this->sectionSize;
        for(size_t i = 0; i < sectionsCountToRemove; ++i){
            this->sections.remove(this->sections.getSize() - 1);
        }

        return result;
    };
};