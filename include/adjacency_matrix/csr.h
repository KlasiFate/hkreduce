#pragma once
#include <utility>
#include <stdexcept>

#include "./abc.h"
#include "../collections/abc.h"


template<class TCoef = float>
class _CSREdgeIterator;


template<class TCoef = float>
class _CSRNeighboursIterator;


template <class TCoef = float>
class CSRAdjacencyMatrix : public ABCAdjacencyMatrix<TCoef> {
private:
    Collection<std::size_t>* _rows;
    Collection<TCoef>* _cols;
    Collection<TCoef>* _coefs;
    bool _delete_collections;

public:
    CSRAdjacencyMatrix(){}

    CSRAdjacencyMatrix(
        Collection<std::size_t>* rows,
        Collection<TCoef>* _cols,
        Collection<TCoef>* _coefs,
        bool delete_collections
    ) :
        ABCAdjacencyMatrix(rows->size()),
        _rows(rows),
        _cols(cols),
        _coefs(coefs),
        _delete_collections(delete_collections) { };

    CSRAdjacencyMatrix(std::size_t size) : ABCAdjacencyMatrix(size), _rows(nullptr), _cols(nullptr), _coefs(nullptr), _delete_collections(true) {
        try {
            this->_rows = new SectionedCollection<std::size_t>(size);
            this->_rows = new SectionedCollection<TCoef>(size);
            this->_rows = new SectionedCollection<TCoef>(size);
        }
        catch (...) {
            ~CSRAdjacencyMatrix();
        }
    };

    CSRAdjacencyMatrix(CSRAdjacencyMatrix<TCoef>&& other) : ABCAdjacencyMatrix(other._size), _rows(other._rows), _cols(other._cols), _coefs(other._coefs) {
        other._rows = nullptr;
        other._cols = nullptr;
        other._coefs = nullptr;
    };
    CSRAdjacencyMatrix<TCoef>& operator=(CSRAdjacencyMatrix<TCoef>&& other) {
        delete this->_rows;
        delete this->_cols;
        delete this->_coefs;
        this->_set_size(other._size);
        this->_rows = other._rows;
        this->_cols = other._cols;
        this->_coefs = other._coefs;

        other->_rows = nullptr;
        other->_cols = nullptr;
        other->_coefs = nullptr;
    }

    // TODO: I'm lazy
    CSRAdjacencyMatrix(const CSRAdjacencyMatrix<TCoef>& other) = delete;
    CSRAdjacencyMatrix<TCoef>& operator=(const CSRAdjacencyMatrix<TCoef>& other) = delete;

    ~CSRAdjacencyMatrix() {
        if (this->_delete_collections) {
            delete this->_rows;
            delete this->_cols;
            delete this->_coefs;
        }
    }


    void set_delete_collections(bool delete_collections) {
        this->_delete_collections = delete_collections;
    }
    Collection<std::size_t>* rows() {
        return this->_rows;
    }
    const Collection<std::size_t>* rows() const {
        return this->_rows;
    }
    Collection<TCoef>* cols() {
        return this->_cols;
    }
    const Collection<TCoef>* cols() const {
        return this->_cols;
    }
    Collection<TCoef>* coefs() {
        return this->_coefs;
    }
    const Collection<TCoef>* coefs() const {
        return this->_coefs;
    }

    TCoef get(std::size_t from, std::size_t to) const override {
        if ((this->size() <= from) || (this->size() <= to)) {
            throw std::out_of_range("From or\\and to argument is out of range");
        }
        for (std::size_t idx = from != 0 ? (*this->_rows)[from - 1] : 0; idx < (*this->_rows)[from]; ++idx) {
            std::size_t col = (*this->_cols)[idx];
            if (col == to) {
                return (*this->_coefs)[idx];
            }
            if (col > to) {
                return 0;
            }
        }
        return 0;
    }

    TCoef set(std::size_t from, std::size_t to, TCoef coef) {
        if ((this->size() <= from) || (this->size() <= to)) {
            throw std::out_of_range("From or\\and to argument are out of range");
        }

        std::size_t start = from != 0 ? (*this->_rows)[from - 1] : 0;
        std::size_t stop = (*this->_rows)[from];

        bool set = false;
        for (std::size_t idx = start; idx < stop; ++idx) {
            std::size_t col = (*this->_cols)[idx];
            if (to == col) {
                TCoef old = (*this->_coefs)[idx];
                (*this->_coefs)[idx] = coef;
                return old;
            }
            if (to < col) {
                if (coef == 0) {
                    return;
                }
                this->_cols->insert(idx, to);
                this->_coefs->insert(idx, coef);
                set = true;
                break;
            }
        }

        if (!set) {
            if (coef == 0) {
                return;
            }
            this->_cols->insert(stop, to);
            this->_coefs->insert(stop, coef);
        }
        for (std::size_t idx = from; idx < this->_size; ++idx) {
            ++this->_rows[idx];
        }
        return old;
    }

    NeighboursIterator<TCoef> neighbours_iterator(std::size_t from, std::size_t to) {
        return NeighboursIterator<TCoef>(new _CSRNeighboursIterator<TCoef>(this, from, to));
    };

    void neighbours_iterator(std::size_t from, std::size_t to, NeighboursIterator<TCoef>& to_replace) {
        _CSRNeighboursIterator<TCoef>* iterator = (_CSRNeighboursIterator<TCoef>*) to_replace._iterator();
        (*iterator) = _CSRNeighboursIterator<TCoef>(this, from, to);
    }
};


template <class TCoef>
class _CSRNeighboursIterator : public _ABCNeighboursIterator<TCoef> {
private:
    std::size_t _idx;
    std::size_t _from;
public:
    _CSRNeighboursIterator(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from, std::size_t to) : _ABCNeighboursIterator<TCoef>(matrix, from, to) {
        if (from >= matrix->size() || to >= matrix->size()) {
            throw std::out_of_range("Index is out of range");
        }

        Collection<std::size_t>& rows = *(matrix->rows());
        std::size_t start = this->_from != 0 ? rows[this->_from - 1] : 0;
        std::size_t stop = rows[this->_from];

        for (std::size_t idx = start : 0; idx < stop; ++idx) {
            std::size_t col = cols[idx];
            if (to <= col) {
                this->_idx = idx;
                this->_set_stopped(false);
                return;
            }
        }
        this->_idx = rows[from];
        this->_set_stopped(true);
    }

    std::size_t from() const override {
        return this->_from;
    }
    std::size_t to() const override {
        Collection<std::size_t>* cols = ((CSRAdjacencyMatrix<TCoef>*) this->_matrix)->cols();
        return (*cols)[this->_idx];
    }
    TCoef coef() const override {
        Collection<TCoef>* coefs = ((CSRAdjacencyMatrix<TCoef>*) this->_matrix)->coefs();
        return (*coefs)[this->_idx];
    }
    TCoef set_coef(TCoef coef) const override {
        CSRAdjacencyMatrix<TCoef>* matrix = ((CSRAdjacencyMatrix<TCoef>*) this->_matrix);
        Collection<std::size_t>& cols = *(matrix->cols());
        Collection<TCoef>& coefs = *(matrix->coefs());

        TCoef old = coefs[this->_idx];
        coefs[this->_idx] = coef
            if (coef == 0) {
                this->_set_stopped(true);
            }
        return old;
    }

    _CSRNeighboursIterator<TCoef>& operator++() override {
        CSRAdjacencyMatrix<TCoef>* matrix = ((CSRAdjacencyMatrix<TCoef>*) this->_matrix);
        Collection<std::size_t>& rows = *(matrix->rows());
        Collection<TCoef>& coefs = *(matrix->coefs());

        std::size_t stop = rows[this->_from];
        std::size_t start = this->_from != 0 ? rows[this->_from - 1] : 0;

        if (this->_idx >= stop) {
            return (*this);
        }

        // stopped by operator--
        if (this->_idx == start && this->stopped() && coefs[this->_idx] != 0) {
            this->_set_stopped(false);
            return *this;
        }

        while (++this->_idx < stop) {
            if (coefs[this->_idx] != 0) {
                // in case when prev coef was set to 0
                this->_set_stopped(false);
                return (*this);
            }
        }
        this->_set_stopped(true);
        return (*this);
    }

    _CSRNeighboursIterator<TCoef>& operator--() override {
        CSRAdjacencyMatrix<TCoef>* matrix = ((CSRAdjacencyMatrix<TCoef>*) this->_matrix);
        Collection<std::size_t>& rows = *(matrix->rows());
        Collection<TCoef>& coefs = *(matrix->coefs());

        std::size_t start = this->_from != 0 ? rows[this->_from - 1] : 0;

        while (this->_idx > start) {
            --this->_idx;
            if (coefs[this->_idx] != 0) {
                // in case when prev coef was set to 0
                this->_set_stopped(false);
                return (*this);
            }
        }
        this->_set_stopped(true);
        return (*this);
    }
};