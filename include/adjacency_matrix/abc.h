#pragma once
#include <cstddef>
#include <stdexcept>


template<class TCoef = float>
class NeighboursIterator;


template <class TCoef = float>
class ABCAdjacencyMatrix {
private:
    std::size_t _size;

protected:
    void _set_size(std::size_t size){
        this->_size = size;
    }

    ABCAdjacencyMatrix(std::size_t size) : _size(size) {
        if (size < 1) {
            throw std::invalid_argument("Size can't less than 1");
        }
    };
public:
    std::size_t size() const {
        return this->_size;
    }

    virtual ~ABCAdjacencyMatrix() { };

    // operator[] is very fast way to get
    virtual TCoef get(std::size_t from, std::size_t to) const = 0;
    virtual TCoef set(std::size_t from, std::size_t to, TCoef get) = 0;

    virtual NeighboursIterator<TCoef> neighbours_iterator(std::size_t from, std::size_t to = 0) = 0;
    virtual void neighbours_iterator(std::size_t from, std::size_t to, NeighboursIterator<TCoef>& to_replace) = 0;
};


template <class TCoef = float>
class _ABCNeighboursIterator {
private:
    bool _stopped;
protected:
    void _set_stopped(bool stopped){
        this->_stopped = stopped;
    }
    _ABCNeighboursIterator(bool stopped) : _stopped(stopped) { };
public:
    virtual _ABCNeighboursIterator<TCoef>* copy() const = 0;
    virtual ~_ABCNeighboursIterator() { };

    bool stopped() const {
        return this->_stopped;
    };
    virtual std::size_t from() const = 0;
    virtual std::size_t to() const = 0;
    virtual TCoef coef() const {
        return this->_matrix->get(this->from(), this->to());
    }
    virtual TCoef set_coef(TCoef coef) const {
        TCoef old = this->coef();
        this->_matrix->set(this->from(), this->to(), coef);
        return old;
    }

    virtual _ABCNeighboursIterator<TCoef>& operator++() = 0;
    virtual _ABCNeighboursIterator<TCoef>& operator--() = 0;
};


template<class TCoef>
class NeighboursIterator : protected std::shared_ptr<_ABCNeighboursIterator<TCoef>> {
public:
    NeighboursIterator(const NeighboursIterator<TCoef>& other): std::shared_ptr(other) {}
    NeighboursIterator(NeighboursIterator<TCoef>&& other): std::shared_ptr(other) {}
    NeighboursIterator& operator=(const NeighboursIterator<TCoef>& other){
        NeighboursIterator<_ABCNeighboursIterator<TCoef>>::operator=(this, other);
        return *this;
    }
    NeighboursIterator& operator=(NeighboursIterator<TCoef>&& other){
        NeighboursIterator<_ABCNeighboursIterator<TCoef>>::operator=(this, other);
        return *this;
    }

    NeighboursIterator() : std::shared_ptr(nullptr) {}
    NeighboursIterator(_ABCNeighboursIterator<TCoef>* iterator) : std::shared_ptr(iterator) { };
    ~NeighboursIterator() { };

    NeighboursIterator<TCoef> operator++(int) {
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        _ABCNeighboursIterator<TCoef>* old = iterator->copy();
        ++(*iterator);
        return NeighboursIterator<TCoef>(old);
    }
    NeighboursIterator<TCoef>& operator++() {
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        ++(*iterator);
        return *this;
    }
    NeighboursIterator<TCoef> operator--(int) {
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        _ABCNeighboursIterator<TCoef>* old = iterator->copy();
        --(*iterator);
        return NeighboursIterator<TCoef>(old);
    }
    NeighboursIterator<TCoef>& operator--() {
        _ABCNeighboursIterator<TCoef>* iterator = this->get();
        --(*iterator);
        return *this;
    }

    bool stopped() const {
        return this->get()->stopped();
    };
    std::size_t from() const {
        return this->get()->from();
    }
    std::size_t to() const {
        return this->get()->to();
    }
    TCoef coef() const {
        return this->get()->coef();
    }
    TCoef set_coef(TCoef coef) {
        return this->get()->set_coef(coef);
    }

private:
    friend void ABCAdjacencyMatrix<TCoef>::neighbours_iterator(std::size_t from, std::size_t neighbor, NeighboursIterator<TCoef>& to_replace);
    _ABCNeighboursIterator<TCoef>* _iterator() const {
        return this->get();
    }
};