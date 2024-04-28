#pragma once
#include <cstddef>

#include "errors.h"

#define DEFAULT_SECTION_SIZE 1024
#define DEFAULT_SIZE_INCREMENTER 1024

template <class T, class TSize = std::size_t>
class ABCVector {
protected:
    TSize _size = 0;

    ABCVector(TSize size) : _size(size) { };

public:
    inline TSize get_size() const {
        return this->_size;
    };

    virtual ~ABCVector() { };

    // operator[] is very fast way to access
    virtual T& operator[](TSize idx) const;
    virtual T& at(TSize idx) const = 0;

    virtual TSize append(const T& value) = 0;
    virtual TSize append(T&& value) = 0;

    virtual void insert(TSize idx, const T& value) = 0;
    virtual void insert(TSize idx, T&& value) = 0;

    virtual T pop(TSize idx) = 0;
    void remove(TSize idx) {
        this->pop(idx);
    }

    virtual void approximately_increase(TSize size) = 0;
    virtual void resize(TSize new_allocated_size) = 0;
    virtual void truncate() {
        this->resize(this->_size);
    };

    virtual ABCContainerIterator<T, TSize> iterator(TSize idx = 0) const = 0;
    virtual ABCContainerIterator<T, TSize> reversed_from_end() const {
        return this->v_iterator(this->_size - 1);
    };
};


template <class T, class TSize = std::size_t>
class ABCContainerIterator {
protected:
    bool _stopped = false;
    VectorIterator(bool stopped) : _stopped(stopped) { };

public:
    virtual T& operator*() = 0;

    inline bool stopped() {
        return this->_stopped;
    };

    virtual ABCContainerIterator<T, TSize>& operator++() = 0;
    virtual ABCContainerIterator<T, TSize> operator++(int) = 0;
    virtual ABCContainerIterator<T, TSize>& operator--() = 0;
    virtual ABCContainerIterator<T, TSize> operator--(int) = 0;
};

