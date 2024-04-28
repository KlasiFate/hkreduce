#pragma once
#include <utility>

#include "containers/abc.h"

#define DEFAULT_SIZE_INCREMENTOR 1024

template <class T, class TSize = std::size_t, TSize SIZE_INCREMENTOR = DEFAULT_SIZE_INCREMENTOR>
class Vector : public ABCVector<T, TSize> {
protected:
    T* _elements = nullptr;
    TSize _allocated = 0;
public:
    Vector(TSize preallocate = 0) : _allocated(preallocate) {
        if (preallocate < 0) {
            throw ValueError("\"preallocate\" argument is less then zero");
        }
        else if (preallocate != 0) {
            this->_elements = new T[preallocate];
        }
    };
    Vector(const Vector<T, TSize>& other) : _allocated(other._allocated), _size(other._size) {
        this->_elements = new T[this->_allocated];
        for (TSize pos = 0; pos < other._size; ++pos) {
            this->_elements[pos] = other._elements[pos];
        }
    };
    Vector(Vector<T, TSize>&& other) : _allocated(other._allocated), _size(other._size), _elements(other._elements) {
        other._elements = nullptr;
    };

    ~Vector() {
        delete this->_elements;
    }

    Vector<T, TSize>& operator=(const Vector<T, TSize>& other) {
        delete this->_elements;
        this->_allocated = other._allocated;
        this->_size = other._size;
        this->_elements = new T[this->_allocated];
        for (TSize pos = 0; pos < other._size; ++pos) {
            this->_elements[pos] = other._elements[pos];
        }
        return *this;
    };
    Vector<T, TSize>& operator=(Vector<T, TSize>&& other) {
        delete this->_elements;
        this->_allocated = other._allocated;
        this->_size = other._size;
        this->_elements = other._elements;
        other._elements = nullptr;
        return *this;
    };

    T& operator[](TSize idx) const {
        if(idx >= this->_size || idx < 0){
            throw ValueError("Index is out of range");
        }
        return this->_elements[idx];
    }

    void approximately_increase(TSize size) {
        size = (size / SIZE_INCREMENTOR + 1) * SIZE_INCREMENTOR;
        if (size <= this->_allocated) { return; }
        this->resize(size);
    }
    void resize(TSize new_allocated_size) {
        if (this->_allocated == new_allocated_size) { return; }
        if (this->_size < new_allocated_size) {
            throw ValueError("Index is out of range.");
        };
        T* new_elements = new T[new_allocated_size];
        this->_allocated = new_allocated_size;
        for (TSize idx = 0; idx < this->_size; ++idx) {
            new_elements[idx] = std::move(this->_elements[idx]);
        }
    }

protected:
    inline T& _get_space_to_insert(TSize idx) {
        if (idx > this->_size || idx < 0) {
            throw ValueError("Index is out of range.");
        }
        if (this->_size == this->_allocated) {
            this->resize(this->_size + SIZE_INCREMENTOR);
        }
        for (TSize pos = this->_size; pos > idx; --pos) {
            this->_elements[pos] = std::move(this->_elements[pos - 1]);
        }
        return this->_elements[pos];
    }
public:
    TSize append(const T& value) {
        this->_get_space_to_insert(this->_size) = value;
        return this->_size++;
    }
    TSize append(T&& value) {
        this->_get_space_to_insert(this->_size) = value;
        return this->_size++;
    }

    void insert(TSize idx, const T& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }
    void insert(TSize idx, T&& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }

    virtual T pop(TSize idx, bool decrease_space = true){
        T res = std::move((*this)[idx]);
        --this->_size;
        for(TSize pos = idx; pos < this->_size; ++pos){
            this->_elements[pos] = std::move(this->_elements[pos + 1]) 
        }
        if(this->_allocated - this->_size < SIZE_INCREMENTOR){
            this->resize(this->_allocated - (this->_allocated - this->_size) / SIZE_INCREMENTOR * SIZE_INCREMENTOR);
        }
        return res;
    }
    T pop(TSize idx){
        return this->pop(idx, true);
    }

    virtual void remove(TSize idx, bool decrease_space = true){
        this->pop(idx, decrease_space);
    }

    VectorIterator<T, TSize> iterator(TSize idx = 0) const {
        return VectorIterator<T, TSize>(this, idx);
    };
    VectorIterator<T, TSize> iterator_from_end() const {
        return VectorIterator<T, TSize>(this, this->_size - 1);
    };
};

template <class T, class TSize = std::size_t>
class VectorIterator : public ABCContainerIterator {
protected:
    Vector<T, Tsize>* _vector = nullptr;
    TSize _idx = 0;
public:
    VectorIterator() : ABCContainerIterator(true);
    VectorIterator(Vector<T, Tsize>* vector, TSize idx = 0) : _vector(vector), _idx(idx), ABCContainerIterator(idx >= vector->get_size() || idx < 0) { }
    VectorIterator(Vector<T, Tsize>& vector, TSize idx = 0) : VectorIterator(&vector, idx) { };
    VectorIterator(const VectorIterator<T, TSize>& other) : _vector(other._vector), _idx(other._idx), ABCContainerIterator(other._stopped) { }

    VectorIterator<T, TSize>& operator=(const VectorIterator<T, TSize>& other) {
        this->_vector = other._vector;
        this->_idx = other._idx;
        this->_stopped = other._stopped;
    }

    // movement constructor and assignment operator method is such as copying ones

    T& operator*() {
        if (this->_stopped) {
            throw ValueError("Iterator is stopped");
        }
        return this->_vector[idx];
    }
    
    VectorIterator<T, TSize>& operator++() {
        ++this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return *this;
    };
    VectorIterator<T, TSize> operator++(int) {
        VectorIterator<T, TSize> old = &this;
        ++this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return old;
    };
    VectorIterator<T, TSize>& operator--() {
        --this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return *this;
    };
    VectorIterator<T, TSize> operator--(int) {
        VectorIterator<T, TSize> old = &this;
        --this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return old;
    };
};