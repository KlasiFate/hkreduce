#pragma once
#include <utility>
#include <vector>
#include <adjacency_matrix/abc.h>

template <class TCoef = float>
class CSRAdjacencyMatrix : public ABCAdjacencyMatrix<TCoef> {
protected:
    std::vector<std::size_t> _rows;
    std::vector<std::size_t> _cols;
    std::vector<TCoef> _coefs;

public:
    CSRAdjacencyMatrix(std::size_t size = 0) : ABCAdjacencyMatrix(size), _rows(size), _cols(0), _coefs(0) {};
    CSRAdjacencyMatrix(const CSRAdjacencyMatrix& other) : _rows(other._rows), _cols(other._cols), _coefs(other._coefs), ABCAdjacencyMatrix(other._size) { }
    CSRAdjacencyMatrix(CSRAdjacencyMatrix&& other) : _rows(std::move(other._rows)), _cols(std::move(other._cols)), _coefs(std::move(other._coefs)), ABCAdjacencyMatrix(other._size) { }

    CSRAdjacencyMatrix& operator=(const CSRAdjacencyMatrix& other) {
        this->_size = other._size;
        this->_rows = other._rows;
        this->_cols = other._cols;
        this->_coefs = other._coefs;
    }
    CSRAdjacencyMatrix& operator=(CSRAdjacencyMatrix&& other) {
        this->_size = other._size;
        this->_rows = std::move(other._rows);
        this->_cols = std::move(other._cols);
        this->_coefs = std::move(other._coefs);
    }

    std::size_t count_non_zero() const {
        return this->_rows[this->_rows.get_size() - 1];
    };

    TCoef operator[](std::size_t from, std::size_t to) const {
        if ((this->_size <= from) || (from < 0) || (this->_size <= to) || (to < 0)) {
            throw ValueError("Index is out of range;");
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

        TCoef old = 0;
        bool set = false;
        for (std::size_t idx = start; idx < stop; ++idx) {
            std::size_t col = this->_cols[idx];
            if (to > col) {
                break;
            }
            if (to = col) {
                this->_cols.erase(this->_cols.cbegin() + idx);
                old = this->_coefs[idx];
                this->_coefs.erase(this->_coefs.cbegin() + idx);
                set = true;
                break;
            }
        }
        if(set){
            for (std::size_t idx = from; idx < this->_size; ++idx) {
                --this->_rows[idx];
            }
        }
        return old;
    }

    TCoef _set(std::size_t from, std::size_t to, T coef) {
        std::size_t start = from != 0 ? this->_rows[from - 1] : 0;

        TCoef old = 0;
        bool set = false;
        for (std::size_t idx = start; idx < stop; ++idx) {
            std::size_t col = this->_cols[idx];
            if (to = col) {
                TCoef old = this->_coefs[idx];
                this->_coefs[idx] = coef;
                set = true;
                break;
            }
            if (to < col) {
                this->_cols.insert(this->_cols.cbegin() + idx, to);
                this->_coefs.insert(this->_coefs.cbegin() + idx, coef);
                set = true;
                break;
            }
        }

        if (!set) {
            this->_cols.insert(this->_cols.cbegin() + stop, to);
            this->_coefs.insert(this->_coefs.cbegin() + stop, coef);
        }
        for (std::size_t idx = from; idx < this->_size; ++idx) {
            ++this->_rows[idx];
        }
        return old;
    }
public:
    TCoef set(std::size_t from, std::size_t to, TCoef coef, bool remove = false) {
        if ((this->_size <= from) || (this->_size <= to)) {
            throw ValueError("Index is out of range;");
        }
        if (coef == 0 && remove) {
            return this->_set_zero_and_remove(from, to)
        }
        return this->_set(from, to, coef);
    }

    void truncate() {
        this->_rows.shrink_to_fit();
        this->_clos.shrink_to_fit();
        this->_coefs.shrink_to_fit();
    }

    CSREdgeIterator<std::size_t, TCoef> iterator(std::size_t from = 0, std::size_t to = 0){
        return CSREdgeIterator(this, from, to);
    };
};


template <class TSize = std::size_t, class TCoef = float>
class CSREdgeIterator : public ABCEdgeIterator {
protected:
    CSRAdjacencyMatrix<TSize, TCoef>* _matrix;
    TSize _idx = 0;
public:
    CSREdgeIterator(CSRAdjacencyMatrix<TSize, TCoef>* matrix, TSize from = 0, TSize to = 0) : _matrix(matrix), _idx(from * matrix->get_size() + to), ABCEdgeIterator(false) {
        bool stopped = from >= matrix->get_size() || from < 0 || to >= matrix->get_size() || to < 0;
        if (stopped){
            throw ValueError("Index is out of range.")
        }
    };
    CSREdgeIterator(CSRAdjacencyMatrix<TSize, TCoef>& matrix, TSize from = 0, TSize to = 0) : CSREdgeIterator(&matrix, from, to) { };
    CSREdgeIterator(const CSREdgeIterator<TSize, TCoef>& other) : _matrix(other._matrix), _idx(other._idx) { };

    Edge<TSize, TCoef> operator*() {
        if (this->_stopped) {
            throw ValueError("Iterator is stopped");
        }
        TSize from = this->_idx / this->_matrix->get_size();
        TSize to = this->_idx % this->_matrix->get_size();
        
        return Edge(from, to, (*this->_matrix)[from, to]);
    };


    ABCContainerIterator<T, TSize>& operator++() {
        ++this->_idx;
        elements_count = this->_matrix->get_size() * this->_matrix->get_size();
        if (elements_count <= this->_idx) {
            this->_stopped = true;
        }
        else if (this->_idx < 0) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
    ABCContainerIterator<T, TSize> operator++(int){
        ABCContainerIterator<T, TSize> old = *this;
        ++(*this);
        return old;
    };
    ABCContainerIterator<T, TSize>& operator--() {
        --this->_idx;
        elements_count = this->_matrix->get_size() * this->_matrix->get_size();
        if (elements_count <= this->_idx) {
            this->_stopped = true;
        }
        else if (this->_idx < 0) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
    ABCContainerIterator<T, TSize> operator--(int){
        ABCContainerIterator<T, TSize> old = *this;
        --(*this);
        return old;
    };
};