#pragma once

#include <cstddef>
#include <stdexcept>


template<class T>
class Collection {
public:
    virtual ~Collection(){};
    virtual std::size_t size() const = 0;
    virtual T& operator[](std::size_t idx) = 0;
    virtual const T& operator[](std::size_t idx) const = 0;
    virtual T& at(std::size_t idx){
        if(idx >= this->size()){
            throw std::out_of_range("Idx is out of range");
        }
        return (*this)[idx];
    }
    virtual const T& at(std::size_t idx) const {
        if(idx >= this->size()){
            throw std::out_of_range("Idx is out of range");
        }
        return (*this)[idx];
    }
    virtual T replace(std::size_t idx, const T& element) = 0;
    virtual T replace(std::size_t idx, T&& element) = 0;
    virtual void insert(std::size_t idx, const T& element) = 0;
    virtual void insert(std::size_t idx, T&& element) = 0;
    virtual void append(const T& element) {
        this->insert(this->size(), element);
    };
    virtual void append(T&& element) {
        this->insert(this->size(), element);
    };
    virtual T remove(std::size_t idx) = 0;
};