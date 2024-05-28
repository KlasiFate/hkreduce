#pragma once

#include <utility>
#include <vector>
#include "errors.h"
#define _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE 1024


template<class T>
class _SectionedVector {
protected:
    std::vector<std::vector<T>> _values;
    std::size_t _size = 0;
    std::size_t _section_size = 0;

public:
    _SectionedVector(
        std::size_t section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE
    ) : _values(0),
        _section_size(section_size),
        _size(0) {
        if (section_size < 1) {
            throw ValueError<>("Section size argument can't be less than 1.");
        }
    };

    _SectionedVector(
        std::size_t allocate,
        std::size_t section_size,
        const T& value
    ) :
        _values(allocate / section_size + (allocate % section_size != 0 ? 1 : 0), std::vector<T>(0)),
        _section_size(section_size),
        _size(allocate) {
        if (section_size < 1) {
            throw ValueError<>("Section size argument can't be less than 1.");
        }
        for(std::size_t idx = 0; idx < this->_values.size(); ++idx){
            this->_values[idx] = std::vector<T>(section_size, value);
        }
    }


    _SectionedVector(const _SectionedVector<T>& other) :
        _size(other._size),
        _section_size(other._section_size),
        _values(other._values) {};
    
    _SectionedVector(_SectionedVector<T>&& other) :
        _size(other._size),
        _section_size(other._section_size),
        _values(std::move(other._values)) { };

    _SectionedVector<T>& operator=(const _SectionedVector<T>& other) {
        this->_size = other._size;
        this->_section_size = other._section_size;
        this->_values = other._values;
    }

    _SectionedVector<T> operator=(_SectionedVector<T>&& other) {
        this->_size = other._size;
        this->_section_size = other._section_size;
        this->_values = std::move(other._values);
    }

    std::size_t size() const {
        return this->_size;
    }

    T& operator[](std::size_t idx) {
        if (idx >= this->_size) {
            throw ValueError<>("Index is out of range");
        }
        return this->_values[idx / this->_section_size][idx % this->_section_size];
    }

    const T& operator[](std::size_t idx) const {
        if (idx >= this->_size) {
            throw ValueError<>("Index is out of range");
        }
        return this->_values[idx / this->_section_size][idx % this->_section_size];
    }

    void resize(std::size_t preallocate) {
        if (this->_size > preallocate) {
            throw ValueError<>("Requested size space is not enough for elements");
        }
        std::size_t new_sections_count = preallocate / this->_section_size + (preallocate % this->_section_size != 0 ? 1 : 0);
        std::size_t old_sections_count = this->_values.size();
        this->_values.resize(new_sections_count, std::vector<T>(0));
        if(new_sections_count > old_sections_count){
            for(std::size_t idx = old_sections_count; idx < new_sections_count; ++idx){
                this->_values[idx] = std::vector<T>(this->_section_size);
            }
        }

        
    }
    void truncate() {
        this->resize(this->_size);
    }

protected:
    T& _get_space_to_insert(const std::size_t idx) {
        if (idx > this->_size) {
            throw ValueError<>("Index is out of range.");
        }
        if (this->_size == this->_values.size() * this->_section_size) {
            this->resize(this->_size + this->_section_size);
        }
        for (std::size_t pos = this->_size; pos > idx; --pos) {
            T& space = this->_values[pos / this->_section_size][pos % this->_section_size];
            space = std::move(this->_values[(pos - 1) / this->_section_size][(pos - 1) % this->_section_size]);
        }
        return this->_values[idx / this->_section_size][idx % this->_section_size];
    }

public:
    void insert(const std::size_t idx, const T& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }
    void insert(const std::size_t idx, T&& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }
    void append(T&& value) {
        this->insert(this->_size, value);
    }
    void append(const T& value) {
        this->insert(this->_size, value);
    }

    T pop(const std::size_t idx = 0, bool remove = false) {
        if (idx >= this->_size) {
            throw ValueError<>("Index is out of range.");
        }

        T res = std::move(this->_values[idx / this->_section_size][idx % this->_section_size]);

        for (std::size_t pos = idx; pos < this->_size; ++pos) {
            T& space = this->_values[pos / this->_section_size][pos % this->_section_size];
            space = std::move(this->_values[(pos + 1) / this->_section_size][(pos + 1) % this -> _section_size]);
        }

        --this->_size;

        if (remove && this->_size % this->_section_size == 0) {
            this->resize(this->_size);
        }

        return res;
    }
};