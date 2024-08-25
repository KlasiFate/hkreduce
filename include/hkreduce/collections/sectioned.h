#pragma once

#include "../allocators/default.h"
#include "./abc.h"
#include "./array_based.h"
#include "./constants.h"


template<class T>
class SectionedCollection : public IndexableCollection<T> {
public:
    typedef DArrayCollection<T*> Sections;

protected:
    Sections sections;
    size_t sectionSize;
    size_t initializedElements;
    bool collectionInitialized;

public:
    SectionedCollection() :
        IndexableCollection(0),
        sections(),
        sectionSize(0),
        initializedElements(0),
        collectionInitialized(false) { }

private:
    void allocateSections(size_t allocated, Allocator* allocator){
        size_t sectionsCount = allocated / this->sectionSize + (allocated % this->sectionSize == 0 ? 0 : 1);
        size_t allocatedSizeOfArray = (sectionsCount / 1024 + (sectionsCount % 1024 == 0 ? 0 : 1)) * 1024;
        this->sections = Sections(
            allocator->allocate(sizeof(T*) * allocatedSizeOfArray),
            allocatedSizeOfArray,
            sectionsCount,
            true,
            1024,
            allocator
        );
        size_t wasAllocated = 0;
        try {
            for (; wasAllocated < sectionsCount; ++wasAllocated) {
                this->sections[wasAllocated] = allocator->allocate(sizeof(T) * sectionSize);
            }
        }
        catch (const bad_alloc& error) {
            for (size_t i = 0; i < wasAllocated; ++i) {
                allocator->deallocate(this->sections[i]);
            }
            throw;
        }
    }

public:
    SectionedCollection(
        size_t allocated,
        Allocator* allocator = getDefaultAllocator(),
        size_t sectionSize = DEFAULT_BLOCK_SIZE
    ) :
        IndexableCollection(0),
        sections(),
        sectionSize(sectionSize),
        initializedElements(0),
        collectionInitialized(true) {
        if (sectionSize == 0) {
            throw new invalid_argument("Section size equals zero");
        }
        if (allocator == nullptr) {
            throw new invalid_argument("Allocator can't equal nullptr")
        }
        this->allocateSections(allocated, allocator);
    }

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(
        size_t size,
        const T& value,
        Allocator* allocator = getDefaultAllocator(),
        size_t sectionSize = DEFAULT_BLOCK_SIZE
    ) : SectionedCollection(size, allocator, sectionSize) {
        // "this->initializedElements" в цикле используется для поддержания в актуальном состоянии данного показателя
        // на случай, если в одном из конструкторов T произойдет ошибка и раскрутка стека удалит коллекцию
        for (; this->initializedElements < size; ;) {
            size_t sectionIdx = this->initializedElements / this->sectionSize;
            size_t idxInSection = this->initializedElements % this->sectionSize;
            new (this->sections[sectionIdx] + idxInSection) T(value);
            this->setSize(++this->initializedElements);
        }
    };

    template<enable_if_t<!is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(const SectionedCollection<T>& other) = delete;

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection(const SectionedCollection<T>& other) :
        IndexableCollection(0),
        sections(),
        sectionSize(other.sectionSize),
        initializedElements(other.initializedElements),
        collectionInitialized(other.collectionInitialized) {
        if (!other.collectionInitialized) {
            return;
        }
        this->allocateSections(other.getSize(), other.getAllocator());

        for (; this->initializedElements < size; ;) {
            size_t sectionIdx = this->initializedElements / this->sectionSize;
            size_t idxInSection = this->initializedElements % this->sectionSize;
            new (this->sections[sectionIdx] + idxInSection) T(other->sections[sectionIdx] + idxInSection);
            this->setSize(++this->initializedElements);
        }
    }

    SectionedCollection(SectionedCollection<T>&& other) :
        IndexableCollection(other.getSize()),
        sections(move(other.sections)),
        initializedElements(other.initializedElements),
        sectionSize(other.sectionSize),
        collectionInitialized(other.collectionInitialized) { };

    template<enable_if_t<!is_copy_constructible<T>::value, bool> = true>
    SectionedCollection& operator=(const SectionedCollection<T>& other) = delete;

    template<enable_if_t<is_copy_constructible<T>::value, bool> = true>
    SectionedCollection& operator=(const SectionedCollection<T>& other) {
        if (this == &other) {
            return;
        }

        if (this->collectionInitialized) {
            if (!is_trivial<T>::value) {
                for (size_t i = 0; i < this->initializedElements; ++i) {
                    (*this)[i].~T();
                }
            }
            this->initializedElements = 0;
            this->setSize(0);
        }

        if(!other.collectionInitialized && this->collectionInitialized){
            for (size_t i = 0; i < this->sections.getSize(); ++i) {
                this->sections->getAllocator()->deallocate(this->sections[i]);
            }
            this->sections = other.sections();
            this->collectionInitialized = false;
            return;
        }

        if(!this->collectionInitialized){
            this->sectionSize = other.sectionSize;
            this->allocateSections(other.getSize(), other.getAllocator());
            this->collectionInitialized = true;
        }else{
            size_t newSectionsCount = other.getSize() / this->sectionSize + (other.getSize() % this->sectionSize == 0 ? 0 : 1);
            this->resize(newSectionsCount * this->sectionSize);
        }

        for(size_t i = 0; i < other.getSize(); ++i){
            size_t sectionIdx = idx / this->getSectionSize();
            size_t idxInSection = idx % this->getSectionSize();
            new (this->sections[sectionIdx] + idxInSection) T(other->sections[sectionIdx] + idxInSection);
        }
    }

    SectionedCollection& operator=(SectionedCollection<T>&& other) {
        if (this == &other) {
            return;
        }

        if (!is_trivial<T>::value) {
            for (size_t i = 0; i < this->initializedElements; ++i) {
                (*this)[i].~T();
            }
        }
        for (size_t i = 0; i < this->sections.getSize(); ++i) {
            this->sections->getAllocator()->deallocate(this->sections[i]);
        }

        this->sections = move(other.sections);
        this->initializedElements = other.initializedElements;
        this->setSize(other.getSize());
        this->sectionSize = other.sectionSize;
    }

    ~SectionedCollection() {
        if (!is_trivial<T>::value) {
            for (size_t i = 0; i < this->initializedElements; ++i) {
                (*this)[i].~T();
            }
        }
        for (size_t i = 0; i < this->sections.getSize(); ++i) {
            this->sections->getAllocator()->deallocate(this->sections[i]);
        }
    }

    Allocator* getAllocator() const {
        return sections->getAllocator();
    }

    T& operator[](size_t idx) override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / this->getSectionSize();
        size_t idxInSection = idx % this->getSectionSize();
        return this->sections[sectionIdx][idxInSection];
    };
    const T& operator[](size_t idx) const override {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        size_t sectionIdx = idx / this->getSectionSize();
        size_t idxInSection = idx % this->getSectionSize();
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
        this->sections[sectionIdx][idxInSection] = element;
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

    void resize(size_t newSpaceSize) {
        if (newSpaceSize < this->getSize()) {
            throw out_of_range("newSpaceSize argument is less than current size");
        }

        size_t newSectionsCount = newSpaceSize / this->sectionSize + (newSpaceSize % this->sectionSize == 0 ? 0 : 1);

        if (newSectionsCount == this->sections.getSize()) {
            return;
        }
        if (newSectionsCount > this->sections.getSize()) {
            for (size_t i = 0; i < newSectionsCount - this->sections.getSize(); ++i) {
                this->sections.append(this->sections.getAllocator()->allocate(sizeof(T) * this->sectionSize));
            }
            return;
        }

        size_t fullInitializedSectionsCount = this->initializedElements / this->sectionSize;
        size_t remainingInitializedElementsCount = this->initializedElements % this->sectionSize;

        size_t sectionsCountToRemove = this->sections.getSize() - newSectionsCount;
        for (size_t i = 0; i < sectionsCountToRemove; ++i) {
            size_t sectionIdx = this->sections.getSize() - 1;
            T* section = this->sections.remove(sectionIdx);
            if (sectionIdx < fullInitializedSectionsCount) {
                if (!is_trivial<T>::value) {
                    for (size_t i = 0; i < this->sectionSize; ++i) {
                        section[i].~T();
                    }
                }
                this->initializedElements -= this->sectionSize;
            }
            else if (sectionIdx == fullInitializedSectionsCount) {
                if (!is_trivial<T>::value) {
                    for (size_t i = 0; i < remainingInitializedElementsCount; ++i) {
                        section[i].~T();
                    }
                }
                this->initializedElements -= remainingInitializedElementsCount;
            }
            this->sections.getAllocator()->deallocate((void*) section);
        }
    }

private:
    void insert(size_t idx, function<void(T*)> assignOrConstruct) {
        if (idx > this->getSize()) {
            throw out_of_range("idx argument is out of range");
        }
        if (this->getSize() == this->sectionSize * this->sections.getSize()) {
            this->resize(this->getSize() + this->sectionSize);
        }

        // TODO: optimize for trivial types

        if (this->getSize() == this->wasInitalized) {
            size_t sectionIdx = this->getSize() / this->sectionSize;
            size_t idxInSection = this->getSize() % this->sectionSize;
            if (this->getSize() == idx) {
                assignOrConstruct(this->sections[sectionIdx] + idxInSection, false);
            }
            else {
                size_t sectionIdx2 = (this->getSize() - 1) / this->sectionSize;
                size_t idxInSection2 = (this->getSize() - 1) % this->sectionSize;
                new (this->sections[sectionIdx] + idxInSection) T(move(*(this->sections[sectionIdx2] + idxInSection2)));
                for (size_t i = this->getSize() - 1; i > idx; --i) {
                    sectionIdx = i / this->sectionSize;
                    sectionIdx2 = (i - 1) / this->sectionSize;
                    idxInSection = i % this->sectionSize;
                    idxInSection2 = (i - 1) % this->sectionSize;

                    *(this->sections[sectionIdx] + idxInSection) = move(*(this->sections[sectionIdx2] + idxInSection2));
                }
                sectionIdx = idx / this->sectionSize;
                idxInSection = idx % this->sectionSize;
                assignOrConstruct(this->sections[sectionIdx] + idxInSection, true);
            }

            this->setSize(this->getSize() + 1);
            ++this->wasInitalized;
            return;
        }

        for (size_t i = this->getSize(); i > idx; --i) {
            size_t sectionIdx = i / this->sectionSize;
            size_t sectionIdx2 = (i - 1) / this->sectionSize;
            size_t idxInSection = i % this->sectionSize;
            size_t idxInSection2 = (i - 1) % this->sectionSize;
            *(this->sections[sectionIdx] + idxInSection) = move(*(this->sections[sectionIdx2] + idxInSection2));
        }
        size_t sectionIdx = idx / this->sectionSize;
        size_t idxInSection = idx % this->sectionSize;
        assignOrConstruct(this->sections[sectionIdx] + idxInSection, true);

        this->setSize(this->getSize() + 1);
    }

public:
    template<enable_if_t<is_copy_assignable<T>::value&& is_copy_constructible<T>::value, bool> = true>
    void insert(size_t idx, const T& element) override {
        const T* elementPtr = &element;
        this->insert(idx, [elementPtr] (T* place, bool assign) -> void {
            if (assign) {
                *place = *elementPtr;
            }
            else {
                new (place) T(*elementPtr);
            }
            })
    };

    void insert(size_t idx, T&& element) override {
        T* elementPtr = &element;
        this->insert(idx, [elementPtr] (T* place, bool assign) -> void {
            if (assign) {
                *place = move(*elementPtr);
            }
            else {
                new (place) T(move(*elementPtr));
            }
            })
    };

    T remove(size_t idx) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        T result(move((*this)[idx]));
        for (size_t i = idx; i < this->getSize() - 1; ++i) {
            size_t sectionIdx = i / this->sectionSize;
            size_t sectionIdx2 = (i + 1) / this->sectionSize;
            size_t idxInSection = i % this->sectionSize;
            size_t idxInSection2 = (i + 1) % this->sectionSize;
            *(this->sections[sectionIdx] + idxInSection) = move(*(this->sections[sectionIdx2] + idxInSection2));
        }
        this->setSize(--this->getSize());

        if (this->getSize() % this->sectionSize == 0) {
            this->resize(this->getSize());
        }

        return result;
    };
};