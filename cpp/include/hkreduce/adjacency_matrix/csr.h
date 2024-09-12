#pragma once

#include <stdexcept>

#include "./abc.h"
#include "../allocators/abc.h"
#include "../allocators/default.h"
#include "../collections/abc.h"
#include "../collections/array_based.h"
#include "../collections/algorithms.h"

using namespace std;

template<class TCoef = float>
class _CSRNeighboursIterator;


template <class TCoef = float>
class CSRAdjacencyMatrix : public ABCAdjacencyMatrix<TCoef> {
private:
    IndexableCollection<size_t>* rows;
    IndexableCollection<size_t>* cols;
    IndexableCollection<TCoef>* coefs;
    Allocator* allocator;
    bool deleteCollections;

public:
    // TODO: add simple constructor based on sectioned collections
    CSRAdjacencyMatrix() :
        ABCAdjacencyMatrix<TCoef>(0),
        rows(nullptr),
        cols(nullptr),
        coefs(nullptr),
        allocator(nullptr),
        deleteCollections(false) { }

    CSRAdjacencyMatrix(
        IndexableCollection<size_t>* rows,
        IndexableCollection<size_t>* cols,
        IndexableCollection<TCoef>* coefs,
        bool deleteCollections = true,
        Allocator* allocator = getDefaultAllocator()
    ) :
        ABCAdjacencyMatrix<TCoef>{rows->getSize()},
        rows(rows),
        cols(cols),
        coefs(coefs),
        allocator(allocator),
        deleteCollections(deleteCollections) { };

    CSRAdjacencyMatrix(
        IndexableCollection<size_t>* rows,
        IndexableCollection<size_t>* cols,
        IndexableCollection<TCoef>* coefs
    ) : CSRAdjacencyMatrix(rows, cols, coefs, true, getDefaultAllocator()) { };

    CSRAdjacencyMatrix(CSRAdjacencyMatrix<TCoef>&& other) :
        ABCAdjacencyMatrix<TCoef>{other.getSize()},
        rows(other.rows),
        cols(other.cols),
        coefs(other.coefs),
        allocator(allocator),
        deleteCollections(other.deleteCollections) {
        other.rows = nullptr;
        other.cols = nullptr;
        other.coefs = nullptr;
        other.deleteCollections = false;
    };

    CSRAdjacencyMatrix<TCoef>& operator=(CSRAdjacencyMatrix<TCoef>&& other) {
        if (this->deleteCollections && this->rows != nullptr) {
            this->rows->~IndexableCollection();
            this->cols->~IndexableCollection();
            this->coefs->~IndexableCollection();

            this->allocator->deallocate((void*) this->rows);
            this->allocator->deallocate((void*) this->cols);
            this->allocator->deallocate((void*) this->coefs);
        }

        this->setSize(other.size);
        this->deleteCollections = other.deleteCollections;
        this->allocator = other.allocator;
        this->rows = other.rows;
        this->cols = other.cols;
        this->coefs = other.coefs;

        other.rows = nullptr;
        other.cols = nullptr;
        other.coefs = nullptr;
        other.deleteCollections = false;
    }

    // TODO: I'm lazy
    CSRAdjacencyMatrix(const CSRAdjacencyMatrix<TCoef>& other) = delete;
    CSRAdjacencyMatrix<TCoef>& operator=(const CSRAdjacencyMatrix<TCoef>& other) = delete;

    ~CSRAdjacencyMatrix() {
        if (this->deleteCollections && this->rows != nullptr) {
            this->rows->~IndexableCollection();
            this->cols->~IndexableCollection();
            this->coefs->~IndexableCollection();

            this->allocator->deallocate((void*) this->rows);
            this->allocator->deallocate((void*) this->cols);
            this->allocator->deallocate((void*) this->coefs);
        }
    }

    bool getDeleteCollections() const {
        return this->deleteCollections;
    }
    void setDeleteCollections(bool delete_collections) {
        this->deleteCollections = delete_collections;
    }

    IndexableCollection<size_t>* getRows() {
        return this->rows;
    }
    const IndexableCollection<size_t>* getRows() const {
        return this->rows;
    }
    IndexableCollection<size_t>* getCols() {
        return this->cols;
    }
    const IndexableCollection<size_t>* getCols() const {
        return this->cols;
    }
    IndexableCollection<TCoef>* getCoefs() {
        return this->coefs;
    }
    const IndexableCollection<TCoef>* getCoefs() const {
        return this->coefs;
    }

    TCoef at(size_t from, size_t to) const override {
        size_t start = from != 0 ? (*this->rows)[from - 1] : 0;
        size_t stop = (*this->rows)[from];
        if (start == stop) {
            return 0;
        }

        size_t idx = bsearchLeftToInsert<size_t>(*(this->cols), to, start, stop);
        if (idx == start) {
            return 0;
        }

        if ((*this->cols)[idx - 1] == to) {
            return (*this->coefs)[idx - 1];
        }

        return 0;
    }

    TCoef setCoef(size_t from, size_t to, TCoef coef) override {
        if ((this->getSize() <= from) || (this->getSize() <= to)) {
            throw out_of_range("From or\\and to argument are out of range");
        }

        IndexableCollection<size_t>& rows = (*this->rows);
        IndexableCollection<size_t>& cols = (*this->cols);
        IndexableCollection<TCoef>& coefs = (*this->coefs);

        size_t start = from != 0 ? rows[from - 1] : 0;
        size_t stop = rows[from];

        size_t idx = bsearchLeftToInsert<size_t>(*(this->cols), to, start, stop);
        if (idx != start && cols[idx - 1] == to) {
            return coefs.replace(idx - 1, coef);
        }
        // else: row is empty or <from, to> pair is not in matrix

        if (coef != 0) {
            cols.insert(idx, to);
            coefs.insert(idx, coef);
            for (size_t i = from; i < this->getSize(); ++i) {
                ++rows[i];
            }
        }
        return 0;
    }

    NeighboursIterator<TCoef> getNeighboursIterator(size_t from, size_t to = 0, Allocator* allocator = nullptr) override {
        if (from >= this->getSize() || to >= this->getSize()) {
            throw out_of_range("Index is out of range");
        }

        if (allocator == nullptr) {
            allocator = getDefaultAllocator();
        }

        _CSRNeighboursIterator<TCoef>* ptr = (_CSRNeighboursIterator<TCoef>*) allocator->allocate(sizeof(_CSRNeighboursIterator<TCoef>));
        new (ptr) _CSRNeighboursIterator<TCoef>(this, from, to);

        return NeighboursIterator<TCoef>::fromInnerIterator(ptr, allocator);
    };

    void replaceNeighboursIterator(size_t from, size_t to, NeighboursIterator<TCoef>& toReplace, Allocator* allocator = nullptr) override {
        if (from >= this->getSize() || to >= this->getSize()) {
            throw out_of_range("Index is out of range");
        }
        _CSRNeighboursIterator<TCoef>* iterator = (_CSRNeighboursIterator<TCoef>*) toReplace.getInnerIterator();
        if (iterator == nullptr) {
            toReplace = this->getNeighboursIterator(from, to, allocator);
            return;
        }
        *iterator = _CSRNeighboursIterator<TCoef>(this, from, to);
    }
};


template <class TCoef>
class _CSRNeighboursIterator : public _ABCNeighboursIterator<TCoef> {
private:
    size_t idx;
    size_t from;

    friend NeighboursIterator<TCoef> CSRAdjacencyMatrix<TCoef>::getNeighboursIterator(size_t, size_t, Allocator*);
    friend void CSRAdjacencyMatrix<TCoef>::replaceNeighboursIterator(size_t, size_t, NeighboursIterator<TCoef>&, Allocator*);
    _CSRNeighboursIterator(CSRAdjacencyMatrix<TCoef>* matrix, size_t from, size_t to) : _ABCNeighboursIterator<TCoef>(false, matrix) {
        IndexableCollection<size_t>& rows = *(matrix->getRows());
        IndexableCollection<size_t>& cols = *(matrix->getCols());
        size_t start = from != 0 ? rows[from - 1] : 0;
        size_t stop = rows[from];

        for (size_t idx = start; idx < stop; ++idx) {
            size_t col = cols[idx];
            if (to <= col) {
                this->idx = idx;
                return;
            }
        }
        this->idx = rows[from];
        this->setStopped(true);
    }

public:
    _CSRNeighboursIterator(const _CSRNeighboursIterator<TCoef>& other) = default;
    _CSRNeighboursIterator<TCoef>& operator=(const _CSRNeighboursIterator<TCoef>& other) = default;

    _CSRNeighboursIterator<TCoef>* copy(Allocator* allocator) const override {
        _CSRNeighboursIterator<TCoef>* ptr = (_CSRNeighboursIterator<TCoef>*) allocator->allocate(sizeof(_CSRNeighboursIterator<TCoef>));
        new (ptr) _CSRNeighboursIterator<TCoef>(*this);
        return ptr;
    }

    size_t getFrom() const override {
        return this->from;
    }
    size_t getTo() const override {
        IndexableCollection<size_t>* cols = static_cast<CSRAdjacencyMatrix<TCoef>*>(this->getMatrix())->getCols();
        return (*cols)[this->idx];
    }
    TCoef getCoef() const override {
        IndexableCollection<TCoef>* coefs = static_cast<CSRAdjacencyMatrix<TCoef>*>(this->getMatrix())->getCoefs();
        return (*coefs)[this->idx];
    }
    TCoef setCoef(TCoef coef) override {
        CSRAdjacencyMatrix<TCoef>* matrix = static_cast<CSRAdjacencyMatrix<TCoef>*>(this->getMatrix());
        IndexableCollection<TCoef>& coefs = *(matrix->getCoefs());

        TCoef old = coefs[this->idx];
        coefs[this->idx] = coef;
        if (coef == 0) {
            this->setStopped(true);
        }
        return old;
    }

    _CSRNeighboursIterator<TCoef>& operator++() override {
        CSRAdjacencyMatrix<TCoef>* matrix = static_cast<CSRAdjacencyMatrix<TCoef>*>(this->getMatrix());
        IndexableCollection<size_t>& rows = *(matrix->getRows());
        IndexableCollection<TCoef>& coefs = *(matrix->getCoefs());

        size_t stop = rows[this->from];
        size_t start = this->from != 0 ? rows[this->from - 1] : 0;

        if (this->idx >= stop) {
            return (*this);
        }

        // stopped by operator--
        if (this->idx == start && this->getStopped() && coefs[this->idx] != 0) {
            this->setStopped(false);
            return *this;
        }

        while (++this->idx < stop) {
            if (coefs[this->idx] != 0) {
                // in case when prev coef was set to 0
                this->setStopped(false);
                return (*this);
            }
        }
        this->setStopped(true);
        return (*this);
    }

    _CSRNeighboursIterator<TCoef>& operator--() override {
        CSRAdjacencyMatrix<TCoef>* matrix = static_cast<CSRAdjacencyMatrix<TCoef>*>(this->getMatrix());
        IndexableCollection<size_t>& rows = *(matrix->getRows());
        IndexableCollection<TCoef>& coefs = *(matrix->getCoefs());

        size_t start = this->from != 0 ? rows[this->from - 1] : 0;

        while (this->idx > start) {
            --this->idx;
            if (coefs[this->idx] != 0) {
                // in case when prev coef was set to 0
                this->setStopped(false);
                return (*this);
            }
        }
        this->setStopped(true);
        return (*this);
    }
};