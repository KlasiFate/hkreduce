#pragma once

#include <cstddef>
#include <stdexcept>

#include "./bool_reference.h"
#include "../allocators/abc.h"
#include "../utils/type_traits.h"

using namespace std;


template<class T, bool supports_copy_semantic = does_support_copy_semantic<T>::value>
class IndexableCollectionTemplate;


template<class T>
class IndexableCollectionTemplate<T, false> {
    // TODO: add docs that move constructor and move operator= of T should be noexcept for strong exception guarantee of collections
    // TODO: add docs that copy operator= should implement strong exception guarantee

    static_assert(does_support_move_semantic<T>::value, "Provided template argument T does not support move semantic");
    
private:
    size_t size;
    Allocator* allocator;

protected:
    void setSize(size_t size){
        this->size = size;
    }
    void setAllocator(Allocator* allocator){
        this->allocator = allocator;
    }

    IndexableCollectionTemplate(size_t size, Allocator* allocator): size(size), allocator(allocator) {};

    IndexableCollectionTemplate(): size(0){};

public:
    virtual ~IndexableCollectionTemplate(){};
    
    size_t getSize() const {
        return this->size;
    };
    
    // Для данного геттера нет поля, тк кол-во аллоцированного пространства можно хранить по-разному
    // и оно может быть вычислено 
    virtual size_t getAllocatedSize() const = 0;

    Allocator* getAllocator() const;
    
    // Не все коллекции предоставляют возможность resize. Так что данный метод является опциональным для реализации
    virtual void resize(size_t size) {
        if(size < this->size){
            throw invalid_argument("Invalid size argument. It is less than size of the collection");
        }
    };
    void truncate() {
        this->resize(this->getSize());
    }

    virtual T& operator[](size_t idx) = 0;
    virtual const T& operator[](size_t idx) const = 0;
    
    // Просто alias для удобства
    T& at(size_t idx){
        return (*this)[idx];
    }
    const T& at(size_t idx) const {
        return (*this)[idx];
    }
    
    virtual T replace(size_t idx, T&& element) = 0;
    
    virtual void insert(size_t idx, T&& element) = 0;
    
    void append(T&& element) {
        this->insert(this->size(), element);
    };
    
    virtual T remove(size_t idx) = 0;
};


template<class T>
class IndexableCollectionTemplate<T, true>: public IndexableCollectionTemplate<T, false> {
protected:
    using IndexableCollectionTemplate<T, false>::IndexableCollectionTemplate;
public:
    virtual T replace(size_t idx, const T& element) = 0;
    
    virtual void insert(size_t idx, const T& element) = 0;
    
    void append(const T& element) {
        this->insert(this->getSize(), element);
    };
};


template<class T>
class IndexableCollection: public IndexableCollectionTemplate<T, does_support_copy_semantic<T>::value> {
protected:
    using IndexableCollectionTemplate<T, does_support_copy_semantic<T>::value>::IndexableCollectionTemplate;
};


template<>
class IndexableCollection<bool> {
private:
    size_t size;
    Allocator* allocator;

protected:
    void setSize(size_t size){
        this->size = size;
    }
    void setAllocator(Allocator* allocator){
        this->allocator = allocator;
    }

    IndexableCollection(size_t size, Allocator* allocator): size(size), allocator(allocator) {}

public:
    IndexableCollection(): size(0){}
    virtual ~IndexableCollection(){};
    
    size_t getSize() const {
        return this->size;
    };
    virtual size_t getAllocatedSize() const = 0;

    Allocator* getAllocator() const {
        return this->allocator;
    };
    
    virtual void resize(size_t size) {
        if(size < this->size){
            throw invalid_argument("Invalid size argument. It is less than size of the collection");
        }
    };
    void truncate() {
        this->resize(this->getSize());
    }

    virtual BoolReference operator[](size_t idx) = 0;
    virtual const BoolReference operator[](size_t idx) const = 0;

    BoolReference at(size_t idx) {
        return (*this)[idx];
    };
    const BoolReference at(size_t idx) const {
        return (*this)[idx];
    };
    
    virtual bool replace(size_t idx, bool element) = 0;
    virtual void insert(size_t idx, bool element) = 0;
    void append(bool element) {
        this->insert(this->getSize(), element);
    };
    virtual bool remove(size_t idx) = 0;
};