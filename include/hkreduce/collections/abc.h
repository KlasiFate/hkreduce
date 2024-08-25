#pragma once

#include <cstddef>
#include <stdexcept>
#include <type_traits>

#include "./bool_reference.h"
#include "../allocators/abc.h"

using namespace std;


template<class T>
class IndexableCollection {
    // TODO: add check for that move constructor and move operator= are noexcept for strong exception guarantee of collections
    // TODO: add docs that copy operator= must implement strong exception guarantee

    static_assert(is_move_assignable<T>::value && is_move_constructible<T>::value, "Provided template class T is not support move semantic");
    
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
    
    // Для данного геттера нет поля, тк кол-во аллоцированного пространства можно хранить по-разному и оно может быть вычислено 
    virtual size_t getAllocatedSize() const = 0;

    Allocator* getAllocator() const;
    
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
    
    virtual T& at(size_t idx){
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        return (*this)[idx];
    }
    virtual const T& at(size_t idx) const {
        if(idx >= this->getSize()){
            throw out_of_range("Idx is out of range");
        }
        return (*this)[idx];
    }
    
    template<enable_if_t<is_copy_assignable<T>::value && is_move_constructible<T>::value, bool> = true>
    virtual T replace(size_t idx, const T& element) = 0;
    virtual T replace(size_t idx, T&& element) = 0;
    
    template<enable_if_t<is_copy_assignable<T>::value && is_copy_constructible<T>::value, bool> = true>
    virtual void insert(size_t idx, const T& element) = 0;
    virtual void insert(size_t idx, T&& element) = 0;
    
    template<enable_if_t<is_copy_assignable<T>::value && is_copy_constructible<T>::value, bool> = true>
    virtual void append(const T& element) {
        this->insert(this->size(), element);
    };
    virtual void append(T&& element) {
        this->insert(this->size(), element);
    };
    
    virtual T remove(size_t idx) = 0;
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

    virtual BoolReference at(size_t idx) {
        return (*this)[idx];
    };
    virtual const BoolReference at(size_t idx) const {
        return (*this)[idx];
    };
    
    virtual bool replace(size_t idx, bool element) = 0;
    virtual void insert(size_t idx, bool element) = 0;
    virtual void append(bool element) {
        this->insert(this->getSize(), element);
    };
    virtual bool remove(size_t idx) = 0;
};