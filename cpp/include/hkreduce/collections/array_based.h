#pragma once
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Woverloaded-virtual" 

#include <cstring>
#include <functional>

#include "../allocators/default.h"
#include "../utils/type_traits.h"
#include "./abc.h"
#include "./constants.h"


using namespace std;

template<class T, bool supports_copy_semantic = does_support_copy_semantic<T>::value>
class ArrayCollection;

template<class T>
class ArrayCollection<T, false> : public IndexableCollection<T> {
protected:
    T* array;
    size_t allocated;
    bool deleteArray;
    size_t initalizedElements;

public:
    ArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        size_t initalizedElements,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator()
    ) : IndexableCollection<T>(size, allocator), array(array), allocated(allocated), deleteArray(deleteArray), initalizedElements(initalizedElements) { };

    ArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator()
    ) : ArrayCollection(array, allocated, size, size, deleteArray, allocator) { };

    ArrayCollection(
        T* array,
        size_t size,
        Allocator* allocator = getDefaultAllocator()
    ) : ArrayCollection(array, size, size, true, allocator) { };

    ArrayCollection(
        size_t allocated,
        Allocator* allocator = getDefaultAllocator()
    ) : ArrayCollection(nullptr, allocated, 0, true, allocator) {
        this->array = (T*) this->getAllocator()->allocate(sizeof(T) * this->allocated);
    }

    ArrayCollection() : ArrayCollection(nullptr, 0, 0, false, nullptr) { }

    ~ArrayCollection() {
        if (this->array == nullptr) {
            return;
        }
        if (!is_trivial<T>::value) {
            for (size_t i = 0; i < this->initalizedElements; ++i) {
                this->array[i].~T();
            }
        }
        if (this->deleteArray) {
            this->getAllocator()->deallocate((void*) this->array);
        }
    }

    ArrayCollection(const ArrayCollection<T, false>& other) = delete;
    ArrayCollection& operator=(const ArrayCollection<T, false>& other) = delete;

    ArrayCollection(ArrayCollection<T, false>&& other) noexcept : ArrayCollection(other.array, other.allocated, other.getSize(), other.initalizedElements, other.deleteArray, other.getAllocator()) {
        other.deleteArray = false;
        other.array = nullptr;
        other.setSize(0);
        other.allocated = 0;
        other.initalizedElements = 0;
        other.setAllocator(nullptr);
    };

    ArrayCollection<T>& operator=(ArrayCollection<T>&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (this->array != nullptr) {
            if (!is_trivial<T>::value) {
                for (size_t i = 0; i < this->initalizedElements; ++i) {
                    this->array[i].~T();
                }
            }

            if (this->deleteArray) {
                this->getAllocator()->deallocate((void*) this->array);
            }
        }

        this->array = other.array;
        this->deleteArray = other.deleteArray;
        this->allocated = other.allocated;
        this->setAllocator(other.getAllocator());
        this->setSize(other.getSize());
        this->initalizedElements = other.initalizedElements;

        other.deleteArray = false;
        other.array = nullptr;
        other.setSize(0);
        other.allocated = 0;
        other.initalizedElements = 0;
        other.setAllocator(nullptr);
    };

    size_t getAllocatedSize() const override {
        return this->allocated;
    }

    void setDeleteArray(bool deleteArray) {
        this->deleteArray = deleteArray;
    }
    bool getDeleteArray() const {
        return this->deleteArray;
    }
    const T* getArray() const {
        return this->array;
    }
    T* getArray() {
        return this->array;
    }

    T& operator[](size_t idx) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        return this->array[idx];
    }

    const T& operator[](size_t idx) const override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        return this->array[idx];
    }

    T replace(size_t idx, T&& element) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        T old(std::move(this->array[idx]));
        this->array[idx] = element;
        return old;
    }

protected:
    void insertUsingFunction(size_t idx, function<void(T*, bool)> assignOrConstruct) {
        if (this->getSize() == this->allocated) {
            throw invalid_argument("No space to insert");
        }
        if (idx > this->getSize()) {
            throw out_of_range("Idx is out of range");
        }

        if (is_trivial<T>::value) {
            memmove(this->array + idx + 1, this->array + idx, sizeof(T) * (this->getSize() - idx));
            assignOrConstruct(this->array + idx, true);
            this->setSize(this->getSize() + 1);
            this->initalizedElements = this->getSize();
            return;
        }

        if (this->getSize() == this->initalizedElements) {
            if (this->getSize() == idx) {
                assignOrConstruct(this->array + idx, false);
                this->setSize(++this->initalizedElements);
                return;
            }

            new (this->array + this->getSize()) T(std::move(this->array[this->getSize() - 1]));
            for (size_t i = this->getSize() - 1; i > idx; --i) {
                this->array[i] = std::move(this->array[i - 1]);
            }
            ++this->initalizedElements;

            try {
                assignOrConstruct(this->array + idx, true);
            }
            catch (...) {
                for (size_t i = idx; i < this->getSize(); ++i) {
                    this->array[i] = std::move(this->array[i + 1]);
                }
                throw;
            }

            this->setSize(this->initalizedElements);
            return;
        }

        for (size_t i = this->getSize(); i > idx; --i) {
            this->array[i] = std::move(this->array[i - 1]);
        }

        try {
            assignOrConstruct(this->array + idx, true);
        }
        catch (...) {
            for (size_t i = idx; i < this->getSize(); ++i) {
                this->array[i] = std::move(this->array[i + 1]);
            }
            throw;
        }

        this->setSize(this->getSize() + 1);
    }

public:
    void insert(size_t idx, T&& element) override {
        T* elementPtr = &element;
        this->insertUsingFunction(idx, [elementPtr] (T* place, bool assign) -> void {
            if (assign) {
                *place = std::move(*elementPtr);
            }
            else {
                new (place) T(std::move(*elementPtr));
            }
            });
    }

    T remove(size_t idx) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }

        T old = std::move(this->array[idx]);

        if (is_trivial<T>::value) {
            memmove(this->array + idx, this->array + idx + 1, sizeof(T) * (this->getSize() - idx - 1));
            this->setSize(this->getSize() - 1);
            this->initalizedElements = this->getSize();
            return old;
        }

        for (size_t i = idx; i < this->getSize() - 1; ++i) {
            this->array[i] = std::move(this->array[i + 1]);
        }
        this->setSize(this->getSize() - 1);
        return old;
    }

    void clear() override {
        if(!is_trivial<T>::value){
            for(size_t i = 0; i < this->initalizedElements; ++i){
                this->array[i].~T();
            }
        }
        this->initalizedElements = 0;
        this->setSize(0);
    }
};


template<class T>
class ArrayCollection<T, true> : public ArrayCollection<T, false> {
public:
    using ArrayCollection<T, false>::ArrayCollection;

    ArrayCollection(
        size_t size,
        const T& value,
        Allocator* allocator = getDefaultAllocator()
    ) : ArrayCollection(size, allocator) {
        try {
            for (; this->initalizedElements < size; ++this->initalizedElements) {
                new (this->array + this->initalizedElements) T(value);
            }
            this->setSize(size);
        }
        catch (...) {
            for (size_t i = 0; i < this->initalizedElements; ++i) {
                this->array[i].~T();
            }
            this->getAllocator()->deallocate(this->array);
            throw;
        }
    }

    ArrayCollection(const ArrayCollection<T, true>& other) : ArrayCollection(nullptr, other.allocated, 0, 0, true, other.getAllocator()) {
        // Создание на основе копирования "пустой" коллекции
        if (other.array == nullptr) {
            this->deleteArray = false;
            return;
        }

        this->array = (T*) this->getAllocator()->allocate(sizeof(T) * this->allocated);

        if (is_trivial<T>::value) {
            memcpy((void*) this->array, (void*) other.array, sizeof(T) * other.getSize());
            return;
        }

        size_t initalizedElements = 0;
        try {
            for (; initalizedElements < other.getSize(); ++initalizedElements) {
                // other.array has type "T*", so we should make element as const
                const T& otherElement = other.array[initalizedElements];
                new (this->array + initalizedElements) T(otherElement);
            }
            this->setSize(other.getSize());
            this->initalizedElements = initalizedElements;
        }
        catch (...) {
            for (size_t i = 0; i < initalizedElements; ++i) {
                this->array[i].~T();
            }
            this->getAllocator()->deallocate((void*) this->array);
            throw;
        }
    };

    ArrayCollection& operator=(const ArrayCollection<T, true>& other) {
        if (this == &other) {
            return *this;
        }

        // Присвоение "пустой" коллекции
        if (other.array == nullptr) {
            if (this->array != nullptr) {
                if (!is_trivial<T>::value) {
                    for (size_t i = 0; i < this->initalizedElements; ++i) {
                        this->array[i].~T();
                    }
                }
                if (this->deleteArray) {
                    this->getAllocator()->deallocate((void*) this->array);
                }
            }

            this->array = nullptr;
            this->allocated = 0;
            this->setAllocator(nullptr);
            this->deleteArray = false;
            this->initalizedElements = 0;
            this->setSize(0);
            return *this;
        }

        T* newArray = (T*) other.getAllocator()->allocate(other.allocated);

        if (is_trivial<T>::value) {
            memcpy((void*) newArray, (void*) other.array, sizeof(T) * other.getSize());
            this->array = newArray;
            this->allocated = other.allocated;
            this->setSize(other.getSize());
            this->initalizedElements = other.getSize();
            this->setAllocator(other.getAllocator());
            this->deleteArray = true;
            return *this;
        }

        size_t initalizedElements = 0;
        try {
            for (; initalizedElements < other.getSize(); ++initalizedElements) {
                // Make element as const
                const T& otherElement = other.array[initalizedElements];
                new (newArray + initalizedElements) T(otherElement);
            }
        }
        catch (...) {
            for (size_t i = 0; i < initalizedElements; ++i) {
                newArray[i].~T();
            }
            other.getAllocator()->deallocate((void*) newArray);
            throw;
        }

        if (this->array != nullptr) {
            if (!is_trivial<T>::value) {
                for (size_t i = 0; i < this->initalizedElements; ++i) {
                    this->array[i].~T();
                }
            }
            if (this->deleteArray) {
                this->getAllocator()->deallocate((void*) this->array);
            }
        }

        this->array = newArray;
        this->allocated = other.allocated;
        this->setSize(other.getSize());
        this->initalizedElements = other.getSize();
        this->setAllocator(other.getAllocator());
        this->deleteArray = true;
        return *this;
    };

    T replace(size_t idx, const T& element) override {
        if (idx >= this->getSize()) {
            throw out_of_range("Idx is out of range");
        }
        T old(std::move(this->array[idx]));
        try {
            this->array[idx] = element;
        }
        catch (...) {
            this->array[idx] = std::move(old);
            throw;
        }
        return old;
    }

    void insert(size_t idx, const T& element) override {
        const T* elementPtr = &element;
        this->insertUsingFunction(idx, [elementPtr] (T* place, bool assign) -> void {
            if (assign) {
                *place = *elementPtr;
            }
            else {
                new (place) T(*elementPtr);
            }
            });
    }
};


template<class T>
class DArrayCollectionCommonMethods : virtual public ArrayCollection<T> {
protected:
    size_t blockSize;

    DArrayCollectionCommonMethods(size_t blockSize) : blockSize(blockSize) { }

public:
    DArrayCollectionCommonMethods<T>& operator=(DArrayCollectionCommonMethods<T>&& other) noexcept {
        ArrayCollection<T>::operator=(other);
        this->blockSize = other.blockSize;
        return *this;
    };

    size_t getBlockSize() const {
        return this->blockSize;
    }

    void setBlockSize(size_t blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size equals zero");
        }
        this->blockSize = blockSize;
    }

    void resize(size_t newSpaceSize) override {
        if (newSpaceSize == this->allocated) {
            return;
        }
        if (newSpaceSize < this->getSize()) {
            throw out_of_range("New space size is less than current size");
        }

        T* newArray = (T*) this->getAllocator()->allocate(sizeof(T) * newSpaceSize);

        if (is_trivial<T>::value) {
            memcpy((void*) newArray, (void*) this->array, sizeof(T) * this->getSize());
            if (this->deleteArray) {
                this->getAllocator()->deallocate((void*) this->array);
            }
            this->allocated = newSpaceSize;
            this->initalizedElements = this->getSize();
            this->array = newArray;
            this->deleteArray = true;
            return;
        }

        for (size_t i = 0; i < this->getSize(); ++i) {
            new (newArray + i) T(std::move(this->array[i]));
        }
        for (size_t i = 0; i < this->initalizedElements; ++i) {
            this->array[i].~T();
        }
        if (this->deleteArray) {
            this->getAllocator()->deallocate((void*) this->array);
        }

        this->allocated = newSpaceSize;
        this->initalizedElements = this->getSize();
        this->array = newArray;
        this->deleteArray = true;
    }

protected:
    size_t calcNewSpaceSize(size_t size) const {
        size_t newSpaceSize = size / this->blockSize + (size % this->blockSize == 0 ? 0 : 1);
        return newSpaceSize * this->blockSize;
    }

    void insertUsingFunction(size_t idx, function<void(T*)> constructAtPlace) {
        if (idx > this->getSize()) {
            throw out_of_range("Idx is out of range");
        }

        size_t newSpaceSize = this->calcNewSpaceSize(this->getSize() + 1);
        T* newArray = (T*) this->getAllocator()->allocate(sizeof(T) * newSpaceSize);

        if (is_trivial<T>::value) {
            memcpy((void*) newArray, (void*) this->array, sizeof(T) * idx);
            constructAtPlace(newArray + idx);
            memcpy((void*) (newArray + idx + 1), (void*) (this->array + idx), sizeof(T) * (this->getSize() - idx));
            if (this->deleteArray) {
                this->getAllocator()->deallocate(this->array);
            }
            this->array = newArray;
            this->allocated = newSpaceSize;
            this->setSize(this->getSize() + 1);
            this->initalizedElements = this->getSize();
            this->deleteArray = true;
            return;
        }

        for (size_t i = 0; i < idx; ++i) {
            new (newArray + i) T(std::move(this->array[i]));
        }
        try {
            constructAtPlace(newArray + idx);
        }
        catch (...) {
            for (size_t i = 0; i < idx; ++i) {
                this->array[i] = std::move(newArray[i]);
                newArray[i].~T();
            }
            this->getAllocator()->deallocate((void*) newArray);
            throw;
        }

        for (size_t i = idx; i < this->getSize(); ++i) {
            new (newArray + i + 1) T(std::move(this->array[i]));
        }

        for (size_t i = 0; i < this->initalizedElements; ++i) {
            this->array[i].~T();
        }
        if (this->deleteArray) {
            this->getAllocator()->deallocate((void*) this->array);
        }

        this->array = newArray;
        this->allocated = newSpaceSize;
        this->setSize(this->getSize() + 1);
        this->initalizedElements = this->getSize();
        this->deleteArray = true;
    }

public:
    void insert(size_t idx, T&& element) override {
        if (this->getSize() < this->allocated) {
            return ArrayCollection<T>::insert(idx, element);
        }
        T* elementPtr = &element;
        this->insertUsingFunction(idx, [elementPtr] (T* place) -> void {new (place) T(std::move(*elementPtr));});
    }

    T remove(size_t idx) override {
        if (this->allocated - this->getSize() + 1 < this->getBlockSize()) {
            return ArrayCollection<T>::remove(idx);
        }
        if (idx >= this->getSize()) {
            throw out_of_range("idx is out of range");
        }

        T old(std::move(this->array[idx]));

        this->setSize(this->getSize() - 1);

        size_t newSpaceSize = this->getSize() / this->getBlockSize();
        if (this->getSize() % this->getBlockSize() > 0) {
            ++newSpaceSize;
        }
        newSpaceSize *= this->getBlockSize();

        T* newArray = (T*) this->getAllocator()->allocate(newSpaceSize);

        if (is_trivial<T>::value) {
            memcpy((void*) newArray, (void*) this->array, sizeof(T) * idx);
            memcpy((void*) (newArray + idx), (void*) (this->array + idx + 1), sizeof(T) * (this->getSize() - idx));
            if (this->deleteArray) {
                this->getAllocator()->deallocate((void*) this->array);
            }
            this->array = newArray;
            this->initalizedElements = this->getSize();
            this->allocated = newSpaceSize;
            this->deleteArray = true;
            return old;
        }

        for (size_t i = 0; i < idx; ++i) {
            new (newArray + i) T(std::move(this->array[i]));
        }
        for (size_t i = idx; i < this->getSize(); ++i) {
            new (newArray + i) T(std::move(this->array[i + 1]));
        }

        for (size_t i = 0; i < this->initalizedElements; ++i) {
            this->array[i].~T();
        }

        if (this->deleteArray) {
            this->getAllocator()->deallocate((void*) this->array);
        }

        this->array = newArray;
        this->initalizedElements = this->getSize();
        this->allocated = newSpaceSize;
        this->deleteArray = true;

        return old;
    }

    void clear() override {
        ArrayCollection<T>::clear();
        this->truncate();
    }
};


template<class T, bool supports_copy_semantic = does_support_copy_semantic<T>::value>
class DArrayCollection;


template<class T>
class DArrayCollection<T, false> : public DArrayCollectionCommonMethods<T> {
public:
    DArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        size_t wasInitalized,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, allocated, size, wasInitalized, deleteArray, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    }

    DArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, allocated, size, deleteArray, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    };

    DArrayCollection(
        T* array,
        size_t size,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, size, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    };

    DArrayCollection(
        size_t allocated,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(allocated, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            this->getAllocator()->deallocate(this->array);
            throw invalid_argument("Block size argument equals zero");
        }
    }

    DArrayCollection(DArrayCollection<T>&& other) noexcept : ArrayCollection<T>(std::move(other)), DArrayCollectionCommonMethods<T>(other.blockSize) { };

    DArrayCollection() : ArrayCollection<T>(), DArrayCollectionCommonMethods<T>(0) { }

    DArrayCollection<T, false>& operator=(DArrayCollection<T, false>&& other) {
        DArrayCollectionCommonMethods<T>::operator=(std::move(other));
        return *this;
    }

    DArrayCollection(const DArrayCollection<T>& other) = delete;

    DArrayCollection<T, false>& operator=(const DArrayCollection<T, false>& other) = delete;
};


template<class T>
class DArrayCollection<T, true> : public DArrayCollectionCommonMethods<T> {
public:
    // 
    // ------ START OF COPIED CODE ------
    // 
    DArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        size_t wasInitalized,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, allocated, size, wasInitalized, deleteArray, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    }

    DArrayCollection(
        T* array,
        size_t allocated,
        size_t size,
        bool deleteArray,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, allocated, size, deleteArray, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    };

    DArrayCollection(
        T* array,
        size_t size,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(array, size, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            throw invalid_argument("Block size argument equals zero");
        }
    };

    DArrayCollection(
        size_t allocated,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(allocated, allocator), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            this->getAllocator()->deallocate(this->array);
            throw invalid_argument("Block size argument equals zero");
        }
    }

    DArrayCollection(DArrayCollection<T>&& other) noexcept : ArrayCollection<T>(std::move(other)), DArrayCollectionCommonMethods<T>(other.blockSize) { };

    DArrayCollection() : ArrayCollection<T>(), DArrayCollectionCommonMethods<T>(0) { }

    DArrayCollection<T, true>& operator=(DArrayCollection<T, true>&& other) {
        DArrayCollectionCommonMethods<T>::operator=(std::move(other));
        return *this;
    }


    // 
    // ------ END OF COPIED CODE ------
    // 

    DArrayCollection(const DArrayCollection<T>& other) : ArrayCollection<T>(other), DArrayCollectionCommonMethods<T>(other.blockSize) { };

    DArrayCollection(
        size_t size,
        const T& value,
        Allocator* allocator = getDefaultAllocator(),
        size_t blockSize = DEFAULT_BLOCK_SIZE
    ) : ArrayCollection<T>(size, value, allocator, blockSize), DArrayCollectionCommonMethods<T>(blockSize) {
        if (blockSize == 0) {
            if (!is_trivial<T>::value) {
                for (size_t i = 0; i < size; ++i) {
                    this->array[i].~T();
                }
            }
            this->getAllocator()->deallocate(this->array);
            throw invalid_argument("Block size argument equals zero");
        }
    }

    DArrayCollection<T, true>& operator=(const DArrayCollection<T, true>& other) {
        ArrayCollection<T>::operator=(other);
        this->blockSize = other.blockSize;
    };

    void insert(size_t idx, const T& element) override {
        if (this->getSize() < this->allocated) {
            return ArrayCollection<T>::insert(idx, element);
        }
        const T* elementPtr = &element;
        this->insertUsingFunction(idx, [elementPtr] (T* place) -> void {new (place) T(*elementPtr);});
    }
};