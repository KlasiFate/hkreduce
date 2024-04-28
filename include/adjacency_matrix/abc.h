#pragma once
#include <cstddef>
#include <tuple>

template <class TCoef = float>
class ABCAdjacencyMatrix {
protected:
    std::size_t _size = 0;

    ABCAdjacencyMatrix(std::size_t size) : _size(size) { };
public:
    inline std::size_t get_size() const {
        return this->_size;
    }

    virtual ~ABCAdjacencyMatrix() { };

    virtual std::size_t count_non_zero() const = 0;

    // operator[] is very fast way to get
    virtual TCoef operator[](std::size_t from, std::size_t to) const = 0;
    virtual TCoef set(std::size_t from, std::size_t to, TCoef coef, bool expand = false) = 0;

    virtual void truncate() { };

    virtual ABCEdgeIterator<TCoef> iterator(std::size_t from = 0, std::size_t to = 0) = 0;
    ABCEdgeIterator<TCoef> iterator_from_end(bool non_zero = false) {
        return this->iterator(this->_size - 1, this->_size - 1);
    }
};

template <class TCoef = float>
using Edge = std::tuple<std::size_t, std::size_t, TCoef>;



template <class TCoef = float>
class ABCEdgeIterator {
protected:
    bool _stopped = false;
    bool _non_zero = false;
    VectorIterator(bool stopped, bool non_zero = false) : _stopped(stopped), _non_zero(non_zero) { };

public:
    virtual Edge<TCoef> operator*() = 0;

    inline bool stopped() {
        return this->_stopped;
    };

    virtual ABCContainerIterator<T>& operator++() = 0;
    virtual ABCContainerIterator<T> operator++(int) = 0;
    virtual ABCContainerIterator<T>& operator--() = 0;
    virtual ABCContainerIterator<T> operator--(int) = 0;
};