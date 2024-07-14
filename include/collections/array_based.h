#pragma once

#include <stdexcept>

#include "./abc.h"
#include "./constants.h"


template<class T>
class BasedOnFixedArrayCollection: public Collection<T>{
protected:
    T* _array;
    std::size_t _allocated;
    std::size_t _size;
    bool _delete_array;

public:
    BasedOnFixedArrayCollection(){}

    BasedOnFixedArrayCollection(
        T* array,
        std::size_t allocated,
        std::size_t size,
        bool delete_array
    ): _array(array), _allocated(allocated), _size(size), _delete_array(delete_array) {};
    BasedOnFixedArrayCollection(
        T* array,
        std::size_t size
    ): BasedOnFixedArrayCollection(array, size, size, true){};

    ~BasedOnFixedArrayCollection(){
        if(this->_delete_array){
            delete[] _delete_array;
        }
    }

    // TODO: I'm lazy
    BasedOnFixedArrayCollection(const BasedOnFixedArrayCollection<T>& other) = delete;
    BasedOnFixedArrayCollection<T>& operator=(const BasedOnFixedArrayCollection<T>& other) = delete;

    BasedOnFixedArrayCollection(BasedOnFixedArrayCollection<T>&& other): _array(other._array), _allocated(other._allocated), _size(other._size), _delete_array(other._delete_array) {
        other._delete_array = false;
        other._array = nullptr;
    }
    BasedOnFixedArrayCollection<T>& operator=(BasedOnFixedArrayCollection<T>&& other){
        if(this._delete_array){
            delete[] this._array;
        }
        this._array = other._array;
        this._size = other._size;
        this._allocated = other._allocated;
        this._delete_array = other._delete_array;

        other._delete_array = false;
        other._array = nullptr;
    };

    void set_delete_array(bool delete_array){
        this->_delete_array = delete_array;
    }
    bool delete_array() const {
        return this->_delete_array;
    }
    std::size_t allocated() const {
        return this->_allocated;
    }
    const T* array() const {
        return this->_array;
    }
    T* array() {
        return this->_array;
    }

    std::size_t size() const override {
        return this->_size;
    };
    T& operator[](std::size_t idx) override {
        return this->_array[idx];
    }
    const T& operator[](std::size_t idx) const override {
        return this->_array[idx];
    }
    T& at(std::size_t idx) override {
        if(idx >= this->_size){
            throw std::out_of_range("Idx arg out of range");
        }
        return (*this)[idx];
    }
    const T& at(std::size_t idx) const override {
        if(idx >= this->_size){
            throw std::out_of_range("Idx is out of range");
        }
        return (*this)[idx];
    }
    T replace(std::size_t idx, const T& element) override {
        if(idx >= this->_size){
            throw std::out_of_range("Idx is out of range");
        }
        T old = std::move(this->_array[idx]);
        this->_array[idx] = element;
        return old;
    }
    T replace(std::size_t idx, T&& element) override {
        if(idx >= this->_size){
            throw std::out_of_range("Idx is out of range");
        }
        T old = std::move(this->_array[idx]);
        this->_array[idx] = element;
        return old;
    }

protected:
    virtual T& _get_space_to_insert(std::size_t idx){
        if(this->_size == this->_allocated){
            throw std::invalid_argument("No space to insert");
        }
        if(idx > this->_size){
            throw std::out_of_range("Idx is out of range");
        }

        for(std::size_t idx2 = this->_size; idx2 > idx; --idx2){
            this->_array[idx2] = std::move(this->_array[idx2 - 1]);
        }
        ++this->_size;
        return this->_array[idx] 
    }

public:
    void insert(std::size_t idx, const T& element) override {
        this->_get_space_to_insert(idx) = element;
    }

    void insert(std::size_t idx, T&& element) override {
        this->_get_space_to_insert(idx) = element;
    }

    T remove(std::size_t idx) override {
        if(idx >= this->_size){
            throw std::out_of_range("Idx is out of range");
        }
        T result = std::move(this->_array[idx]);
        for(std::size_t idx2 = idx; idx2 < this->_size - 1; ++idx2){
            this->_array[idx2] = std::move(this->_array[idx2 + 1]);
        }
        --this->_size;
        return result;
    }

};


template<class T>
class BasedOnArrayCollection: public BasedOnFixedArrayCollection<T> {
public:
    using BasedOnFixedArrayCollection::BasedOnFixedArrayCollection;

    void resize(std::size_t allocated_space_size){
        if(this->allocated_space_size < this->_size){
            throw std::invalid_argument("Allocated space size can't be less than current size")
        }
        T* new_array = new T[allocated_space_size];

        for(std::size_t idx = 0; idx < this->_size; ++idx){
            new_array[idx] = std::move(this->array[idx]);
        }
        if(this->_delete_array){
            delete this->_array;
        }else{
            this->_delete_array = true;
        }
        this->_array = new_array;
        this->_allocated = allocated_space_size;
    }
    virtual void add_space(){
        std::size_t new_space_size;
        if(this->_allocated % DEFAULT_SIZE == 0){
            new_space_size = this->_allocated + DEFAULT_SIZE;
        }else{
            new_space_size = (this->_allocated / DEFAULT_SIZE + 1) * DEFAULT_SIZE;
        }
        this->resize(new_space_size);
    }
    void truncate(){
        this->resize(this->_size);
    }

protected:
    T& _get_space_to_insert(std::size_t idx) override {
        if(idx > this->_size){
            throw std::out_of_range("Idx is out of range");
        }
        if(this->_size == this->_allocated){
            this->add_space();
        }

        for(std::size_t idx2 = this->_size; idx2 > idx; --idx2){
            this->_array[idx2] = std::move(this->_array[idx2 - 1]);
        }
        ++this->_size;
        return this->_array[idx] 
    }
};