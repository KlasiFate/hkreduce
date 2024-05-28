#pragma once
#include <cstddef>
#include <memory>

#include "errors.h"

template<class TCoef = float>
class EdgeIterator;

template<class TCoef = float>
class NeighboursIterator;


template <class TCoef = float>
class ABCAdjacencyMatrix {
protected:
    std::size_t _size = 0;

    ABCAdjacencyMatrix(std::size_t size) : _size(size) { };
public:
    std::size_t size() const {
        return this->_size;
    }

    virtual ~ABCAdjacencyMatrix() { };

    // operator[] is very fast way to get
    virtual TCoef get(std::size_t from, std::size_t to) const = 0;
    virtual TCoef set(std::size_t from, std::size_t to, TCoef coef) = 0;

    virtual NeighboursIterator<TCoef> neighbours_iterator(std::size_t from, std::size_t neighbor = 0) const = 0;

    virtual EdgeIterator<TCoef> iterator(std::size_t from = 0, std::size_t to = 0, bool non_zero = false) const = 0;
};


template <class TCoef = float>
class _ABCEdgeIterator {
protected:
    bool _stopped = false;
    bool _non_zero = false;
    std::size_t _idx = 0;
    const ABCAdjacencyMatrix<TCoef>* _matrix = nullptr;

    _ABCEdgeIterator(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t idx, bool non_zero = false) : _non_zero(non_zero), _idx(idx), _matrix(matrix) { };

    friend EdgeIterator<TCoef>;
    virtual _ABCEdgeIterator<TCoef>* _copy() const = 0;

public:
    virtual ~_ABCEdgeIterator() { };

    bool stopped() const {
        return this->_stopped;
    };
    bool non_zero() const {
        return this->_non_zero;
    }
    std::size_t from() const {
        return this->_idx / this->_matrix->size();
    }
    std::size_t to() const {
        return this->_idx % this->_matrix->size();
    }

    virtual TCoef coef() const {
        if (this->_stopped) {
            throw ValueError<>("Iterator is stopped");
        }
        std::size_t from = this->_idx / this->_matrix->size();
        std::size_t to = this->_idx % this->_matrix->size();

        return this->_matrix->get(from, to);
    }

    virtual _ABCEdgeIterator<TCoef>& operator++() {
        ++this->_idx;
        for (; this->_idx < this->_matrix->size(); ++this->_idx) {
            if (!this->_non_zero) {
                break;
            }
            std::size_t from = this->_idx / this->_matrix->size();
            std::size_t to = this->_idx % this->_matrix->size();

            if (this->_matrix->get(from, to) != 0) {
                break;
            }
        }

        std::size_t elements_count = this->_matrix->size() * this->_matrix->size();
        if (elements_count <= this->_idx) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
    virtual _ABCEdgeIterator<TCoef>& operator--() {
        --this->_idx;
        for (; this->_idx < this->_matrix->size(); --this->_idx) {
            if (!this->_non_zero) {
                break;
            }
            std::size_t from = this->_idx / this->_matrix->size();
            std::size_t to = this->_idx % this->_matrix->size();

            if (this->_matrix->get(from, to) != 0) {
                break;
            }
        }

        std::size_t elements_count = this->_matrix->size() * this->_matrix->size();
        if (elements_count <= this->_idx) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
};


template<class TCoef>
class EdgeIterator : protected std::shared_ptr<_ABCEdgeIterator<TCoef>> {
public:
    EdgeIterator(_ABCEdgeIterator<TCoef>* iterator = nullptr) : std::shared_ptr<_ABCEdgeIterator<TCoef>>(iterator) { };
    ~EdgeIterator() { };

    EdgeIterator<TCoef> operator++(int) {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCEdgeIterator<TCoef>* iterator = this->get();
        _ABCEdgeIterator<TCoef>* old = iterator->_copy();
        ++(*iterator);
        return EdgeIterator<TCoef>(old);
    }
    EdgeIterator<TCoef>& operator++() {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCEdgeIterator<TCoef>* iterator = this->get();
        ++(*iterator);
        return *this;
    }
    EdgeIterator<TCoef> operator--(int) {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCEdgeIterator<TCoef>* iterator = this->get();
        _ABCEdgeIterator<TCoef>* old = iterator->_copy();
        --(*iterator);
        return EdgeIterator<TCoef>(old);
    }
    EdgeIterator<TCoef>& operator--() {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCEdgeIterator<TCoef>* iterator = this->get();
        --(*iterator);
        return *this;
    }

    bool stopped() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->stopped();
    };
    bool non_zero() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->non_zero();
    }
    std::size_t from() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->from();
    }
    std::size_t to() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->to();
    }

    TCoef coef() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->coef();
    }
};


template <class TCoef = float>
class _ABCNeighboursIterator {
protected:
    bool _stopped = false;
    std::size_t _idx = 0;
    const ABCAdjacencyMatrix<TCoef>* _matrix = nullptr;
    std::size_t _from = 0;

    _ABCNeighboursIterator(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from, std::size_t neighbor) : _idx(neighbor), _matrix(matrix), _from(from) { };

    friend NeighboursIterator<TCoef>;
    virtual _ABCNeighboursIterator<TCoef>* _copy() const = 0;
public:
    virtual ~_ABCNeighboursIterator() { };

    bool stopped() const {
        return this->_stopped;
    };

    std::size_t from() const {
        return this->_from;
    }
    std::size_t neighbour() const {
        return this->_idx;
    }
    virtual TCoef coef() const {
        if (this->_stopped) {
            throw ValueError<>("Iterator is stopped");
        }
        return this->_matrix->get(this->_from, this->_idx);
    }

    virtual _ABCNeighboursIterator<TCoef>& operator++() {
        ++this->_idx;
        for (; this->_idx < this->_matrix->size(); ++this->_idx) {
            if (this->_matrix->get(this->_from, this->_idx) != 0) {
                break;
            }
        }

        if (this->_matrix->size() <= this->_idx) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
    virtual _ABCNeighboursIterator<TCoef>& operator--() {
        --this->_idx;
        for (; this->_idx < this->_matrix->size(); --this->_idx) {
            if (this->_matrix->get(this->_from, this->_idx) != 0) {
                break;
            }
        }

        if (this->_matrix->size() <= this->_idx) {
            this->_stopped = true;
        }
        else {
            this->_stopped = false;
        }
        return *this;
    };
};


template<class TCoef>
class NeighboursIterator : protected std::shared_ptr<_ABCNeighboursIterator<TCoef>> {
public:
    NeighboursIterator(_ABCNeighboursIterator<TCoef>* iterator = nullptr) : std::shared_ptr<_ABCNeighboursIterator<TCoef>>(iterator) { };
    ~NeighboursIterator() { };

    NeighboursIterator<TCoef> operator++(int) {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        _ABCNeighboursIterator<TCoef>* old = iterator->_copy();
        ++(*iterator);
        return NeighboursIterator<TCoef>(old);
    }
    NeighboursIterator<TCoef>& operator++() {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        ++(*iterator);
        return *this;
    }
    NeighboursIterator<TCoef> operator--(int) {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        _ABCNeighboursIterator<TCoef>* old = iterator->_copy();
        --(*iterator);
        return NeighboursIterator<TCoef>(old);
    }
    NeighboursIterator<TCoef>& operator--() {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        --(*iterator);
        return *this;
    }

    bool stopped() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->stopped();
    };
    std::size_t from() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->from();
    }
    std::size_t neighbour() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->neighbour();
    }

    TCoef coef() const {
        if (this->get() == nullptr) {
            throw ValueError<>("No iterator");
        }
        return this->get()->coef();
    }
};