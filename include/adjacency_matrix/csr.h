#pragma once
#include <utility>
#include <vector>
#include <adjacency_matrix/abc.h>
#include "_sectioned_vector.h"


template<class TCoef = float>
class _CSREdgeIterator;


template<class TCoef = float>
class _CSRNeighboursIterator;


template <class TCoef = float>
class CSRAdjacencyMatrix : public ABCAdjacencyMatrix<TCoef> {
protected:
    _SectionedVector<std::size_t> _rows;
    _SectionedVector<std::size_t> _cols;
    _SectionedVector<TCoef> _coefs;

public:
    CSRAdjacencyMatrix(
        std::size_t size,
        std::size_t preallocate,
        std::size_t section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE
    ) :
        ABCAdjacencyMatrix<TCoef>(size),
        _rows(size, section_size, 0),
        _cols(preallocate, section_size, 0),
        _coefs(preallocate, section_size, 0) { };

    CSRAdjacencyMatrix(std::size_t size) : CSRAdjacencyMatrix(size, 0) { };

    CSRAdjacencyMatrix(const CSRAdjacencyMatrix& other) = default;
    CSRAdjacencyMatrix(CSRAdjacencyMatrix&& other) = default;

    CSRAdjacencyMatrix& operator=(const CSRAdjacencyMatrix& other) {
        this->_size = other._size;
        this->_rows = other._rows;
        this->_cols = other._cols;
        this->_coefs = other._coefs;
    }
    CSRAdjacencyMatrix operator=(CSRAdjacencyMatrix&& other) {
        this->_size = other._size;
        this->_rows = std::move(other._rows);
        this->_cols = std::move(other._cols);
        this->_coefs = std::move(other._coefs);
    }

    TCoef get(std::size_t from, std::size_t to) const override {
        if ((this->_size <= from) || (this->_size <= to)) {
            throw ValueError<>("Index is out of range;");
        }
        std::size_t idx = 0;
        if (from != 0) {
            idx = this->_rows[from - 1];
        }
        for (; idx < this->_rows[from]; ++idx) {
            std::size_t col = this->_cols[idx];
            if (col == to) {
                return this->_coefs[idx];
            }
            if (col > to) {
                return 0;
            }
        }
        return 0;
    }

protected:
    TCoef _set_zero_and_remove(std::size_t from, std::size_t to) {
        std::size_t start = from != 0 ? this->_rows[from - 1] : 0;
        std::size_t stop = this->_rows[from];

        TCoef old = 0;
        bool set = false;
        for (std::size_t idx = start; idx < stop; ++idx) {
            std::size_t col = this->_cols[idx];
            if (to > col) {
                break;
            }
            if (to == col) {
                this->_cols.pop(idx);
                old = this->_coefs.pop(idx);
                set = true;
                break;
            }
        }
        if (set) {
            for (std::size_t idx = from; idx < this->_size; ++idx) {
                --this->_rows[idx];
            }
        }
        return old;
    }

    TCoef _set(std::size_t from, std::size_t to, TCoef coef) {
        std::size_t start = from != 0 ? this->_rows[from - 1] : 0;
        std::size_t stop = this->_rows[from];

        TCoef old = 0;
        bool set = false;
        for (std::size_t idx = start; idx < stop; ++idx) {
            std::size_t col = this->_cols[idx];
            if (to == col) {
                TCoef old = this->_coefs[idx];
                this->_coefs[idx] = coef;
                set = true;
                break;
            }
            if (to < col) {
                this->_cols.insert(idx, to);
                this->_coefs.insert(idx, coef);
                set = true;
                break;
            }
        }

        if (!set) {
            this->_cols.insert(stop, to);
            this->_coefs.insert(stop, coef);
        }
        for (std::size_t idx = from; idx < this->_size; ++idx) {
            ++this->_rows[idx];
        }
        return old;
    }
public:
    TCoef set(std::size_t from, std::size_t to, TCoef coef, bool remove) {
        if ((this->_size <= from) || (this->_size <= to)) {
            throw ValueError<>("Index is out of range;");
        }
        if (coef == 0 && remove) {
            return this->_set_zero_and_remove(from, to);
        }
        return this->_set(from, to, coef);
    }

    TCoef set(std::size_t from, std::size_t to, TCoef coef) override {
        return this->set(from, to, coef, false);
    }

    void truncate() {
        this->_rows.truncate();
        this->_clos.truncate();
        this->_coefs.truncate();
    }

    EdgeIterator<TCoef> iterator(std::size_t from = 0, std::size_t to = 0, bool non_zero = false) const {
        return EdgeIterator<TCoef>(new _CSREdgeIterator<TCoef>(this, from, to, non_zero));
    };

    NeighboursIterator<TCoef> neighbours_iterator(std::size_t from, std::size_t neighbor = 0) const {
        return NeighboursIterator<TCoef>(new _CSRNeighboursIterator<TCoef>(this, from, neighbor));
    };
};


template <class TCoef>
class _CSREdgeIterator : public _ABCEdgeIterator<TCoef> {
protected:
    // to _copy iterator is out of matrix boundary
    _CSREdgeIterator(const CSRAdjacencyMatrix<TCoef>* matrix, std::size_t idx, bool non_zero = false) : _ABCEdgeIterator<TCoef>(matrix, idx, non_zero) { };

    _ABCEdgeIterator<TCoef>* _copy() const override {
        return new _CSREdgeIterator(static_cast<const CSRAdjacencyMatrix<TCoef>*>(this->_matrix), this->_idx, this->_non_zero);
    }
public:
    _CSREdgeIterator(const CSRAdjacencyMatrix<TCoef>* matrix, std::size_t from = 0, std::size_t to = 0, bool non_zero = false) : _ABCEdgeIterator<TCoef>(matrix, from* matrix->size() + to, non_zero) {
        if (from >= matrix->size() || to >= matrix->size()) {
            throw ValueError<>("Index is out of range.");
        }
        if (non_zero && this->coef() == 0) {
            ++(*this);
        }
    };
};



template <class TCoef>
class _CSRNeighboursIterator : public _ABCNeighboursIterator<TCoef> {
protected:
    _CSRNeighboursIterator(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from, std::size_t neighbor, bool not_check_boundary) : _ABCNeighboursIterator<TCoef>(matrix, from, neighbor) { }

    _ABCNeighboursIterator<TCoef>* _copy() const override {
        return new _CSRNeighboursIterator(this->_matrix, this->_from, this->_idx, true);
    }
public:
    _CSRNeighboursIterator(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from, std::size_t neighbor = 0) : _ABCNeighboursIterator<TCoef>(matrix, from, neighbor) {
        if (from >= matrix->size() || neighbor >= matrix->size()) {
            throw ValueError<>("Index is out of range");
        }
        if (this->coef() == 0) {
            ++(*this);
        }
    }
};