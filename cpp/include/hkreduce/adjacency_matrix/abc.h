#pragma once

#include <cstddef>
#include <stdexcept>

#include "../allocators/abc.h"


using namespace std;

template<class TCoef = float>
class NeighboursIterator;


template <class TCoef = float>
class ABCAdjacencyMatrix {
private:
    size_t size;

protected:
    void setSize(size_t size) {
        this->size = size;
    }

    ABCAdjacencyMatrix(size_t size) : size(size) {};

public:
    size_t getSize() const {
        return this->size;
    }

    ABCAdjacencyMatrix() : size(0) { }

    virtual ~ABCAdjacencyMatrix() { };

    // "at" is more fast way to get coef
    virtual TCoef at(size_t from, size_t to) const = 0;

    virtual TCoef getCoef(size_t from, size_t to) const {
        if (this->size <= from || this->size <= to) {
            throw out_of_range("from or\\and to argument are out of range");
        }
        return this->at(from, to);
    };
    virtual TCoef setCoef(size_t from, size_t to, TCoef get) = 0;

    virtual NeighboursIterator<TCoef> getNeighboursIterator(size_t from, size_t to = 0, Allocator* allocator = nullptr) = 0;

    virtual void replaceNeighboursIterator(size_t from, size_t to, NeighboursIterator<TCoef>& toReplace, Allocator* allocator = nullptr) {
        toReplace = this->getNeighboursIterator(from, to, allocator);
    };
};


template <class TCoef = float>
class _ABCNeighboursIterator {
private:
    bool stopped;
    ABCAdjacencyMatrix<TCoef>* matrix;

protected:
    void setStopped(bool stopped) {
        this->stopped = stopped;
    }
    _ABCNeighboursIterator(bool stopped, ABCAdjacencyMatrix<TCoef>* matrix) : stopped(stopped), matrix(matrix) { };

public:
    virtual _ABCNeighboursIterator<TCoef>* copy(Allocator* allocator) const = 0;
    virtual ~_ABCNeighboursIterator() { };

    bool getStopped() const {
        return this->stopped;
    };
    ABCAdjacencyMatrix<TCoef>* getMatrix() const {
        return this->matrix;
    }
    virtual size_t getFrom() const = 0;
    virtual size_t getTo() const = 0;
    virtual TCoef getCoef() const {
        return this->matrix->getCoef(this->getFrom(), this->getTo());
    }
    virtual TCoef setCoef(TCoef coef) {
        TCoef old = this->getCoef();
        this->matrix->setCoef(this->getFrom(), this->getTo(), coef);
        if (coef == 0) {
            this->setStopped(true);
        }
        return old;
    }

    virtual _ABCNeighboursIterator<TCoef>& operator++() = 0;
    virtual _ABCNeighboursIterator<TCoef>& operator--() = 0;
};


template<class TCoef>
class NeighboursIterator {
private:
    _ABCNeighboursIterator<TCoef>* iterator;
    Allocator* allocator;

public:
    NeighboursIterator(const NeighboursIterator<TCoef>& other) :
        iterator(other.iterator == nullptr ? nullptr : other.iterator->copy(other.allocator)),
        allocator(other.allocator) { }
    NeighboursIterator(NeighboursIterator<TCoef>&& other) :
        iterator(other.iterator),
        allocator(other.allocator) {
        other.iterator = nullptr;
        other.allocator = nullptr;
    }
    NeighboursIterator& operator=(const NeighboursIterator<TCoef>& other) {
        if (&other == this) {
            return *this;
        }

        if (this->iterator != nullptr) {
            this->iterator->~_ABCNeighboursIterator();
            this->allocator->deallocate((void*) this->iterator);
        }

        this->allocator = other.allocator;
        this->iterator = other.iterator->copy(this->allocator);

        return *this;
    }
    NeighboursIterator& operator=(NeighboursIterator<TCoef>&& other) {
        if (&other == this) {
            return *this;
        }

        if (this->iterator != nullptr) {
            this->iterator->~_ABCNeighboursIterator();
            this->allocator->deallocate((void*) this->iterator);
        }

        this->allocator = other.allocator;
        this->iterator = other.iterator;

        other.iterator = nullptr;
        other.allocator = nullptr;

        return *this;
    }

    NeighboursIterator() : iterator(nullptr), allocator(nullptr) { }

    ~NeighboursIterator() {
        if (this->iterator != nullptr) {
            this->iterator->~_ABCNeighboursIterator();
            this->allocator->deallocate((void*) this->iterator);
        }
    };

private:
    NeighboursIterator(
        _ABCNeighboursIterator<TCoef>* iterator,
        Allocator* allocator
    ) :
        iterator(iterator),
        allocator(allocator) { }

public:
    static NeighboursIterator<TCoef> fromInnerIterator(_ABCNeighboursIterator<TCoef>* iterator, Allocator* allocator) {
        return NeighboursIterator<TCoef>(iterator, allocator);
    }
    _ABCNeighboursIterator<TCoef>* getInnerIterator(){
        return this->iterator;
    }

    NeighboursIterator<TCoef> operator++(int) {
        _ABCNeighboursIterator<TCoef>* old = this->iterator->copy(this->allocator);
        ++(*this->iterator);
        return NeighboursIterator<TCoef>(old, this->allocator);
    }
    NeighboursIterator<TCoef>& operator++() {
        ++(*this->iterator);
        return *this;
    }
    NeighboursIterator<TCoef> operator--(int) {
        _ABCNeighboursIterator<TCoef>* old = this->iterator->copy(this->allocator);
        --(*this->iterator);
        return NeighboursIterator<TCoef>(old, this->allocator);
    }
    NeighboursIterator<TCoef>& operator--() {
        --(*this->iterator);
        return *this;
    }

    bool getStopped() const {
        return this->iterator->getStopped();
    };
    size_t getFrom() const {
        return this->iterator->getFrom();
    }
    size_t getTo() const {
        return this->iterator->getTo();
    }
    TCoef getCoef() const {
        return this->iterator->getCoef();
    }
    TCoef setCoef(TCoef coef) {
        return this->iterator->setCoef(coef);
    }
};