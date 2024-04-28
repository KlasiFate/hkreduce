#pragma once
#include "containers/vector.h"

#define DEFAULT_SECTION_SIZE 1024

template <class T, class TSize = std::size_t, TSize SECTION_SIZE = DEFAULT_SECTION_SIZE>
class SectionedVector : public ABCVector<T, TSize> {
protected:
    using Section = Vector<T, TSize, SECTION_SIZE>;
    Vector<Section, TSize> _sections;
public:
    SectionedVector(TSize preallocate = 0) : ABCVector(0) {
        if (preallocate < 0) {
            throw ValueError("\"preallocate\" argument is less than zero.")
        }
        else if (preallocate != 0) {
            TSize sections_count = preallocate / SECTION_SIZE;
            if (preallocate % SECTION_SIZE != 0) {
                ++sections_count;
            }
            this->_sections = Vector(sections_count);
            for (VectorIterator<Section, TSize> iterator = this->_sections.iterator(); !iterator.stopped(); ++iterator) {
                (*iterator).resize(SECTION_SIZE);
            }
        }
    };
    SectionedVector(const SectionedVector& other) : _sections(other._sections), ABCVector(other._size) { }
    SectionedVector(SectionedVector&& other) : _sections(std::move(other._sections)), ABCVector(other._size) { }

    SectionedVector& operator=(const SectionedVector& other) {
        this->_sections = other._sections;
        this->_size = other._size;
    }
    SectionedVector& operator=(SectionedVector&& other) {
        this->_sections = std:move(other._sections);
        this->_size = other._size;
    }

    T& operator[](TSize idx) const {
        if (idx >= this->_size || idx < 0) {
            throw ValueError("Index is out of range");
        }
        return this->_sections[idx / SECTION_SIZE][idx % SECTION_SIZE];
    }

    void approximately_increase(TSize size) {
        TSize sections_count = size / SECTION_SIZE + 1;
        if (sections_count <= this->_sections.get_size()) { return; }
        this->resize(sections_count * SECTION_SIZE);
    }
    void resize(TSize new_allocated_size) {
        if (this->_allocated == new_allocated_size) { return; }
        if (this->_size > new_allocated_size) {
            throw ValueError("New allocated size is less than count of stored elements.");
        }
        TSize sections_count = new_allocated_size / SECTION_SIZE;
        if (new_allocated_size % SECTION_SIZE != 0) {
            ++sections_count;
        }
        this->_sections.resize(sections_count);
        this->_sections[this->_sections.get_size() - 1].resize(new_allocated_size % SECTION_SIZE);
    }

protected:
    inline T& _get_space_to_insert(TSize idx) {
        if (idx > this->_size || idx < 0) {
            throw ValueError("Index is out of range");
        }
        TSize sections_count = this->_sections.get_size();
        if (this->_size == this->_sections.get_size() * SECTION_SIZE) {
            this->resize((sections_count + 1) * SECTION_SIZE)
        }

        for (TSize pos = this->_size; pos > idx; --pos) {
            this->_elements[pos] = std::move(this->_elements[pos - 1]);
        }
        return this->_elements[pos];
    }

public:
    TSize append(const T& value) {
        this->_get_space_to_insert(this->_size) = value;
        return this->_size++;
    }
    TSize append(T&& value) {
        this->_get_space_to_insert(this->_size) = value;
        return this->_size++;
    }

    void insert(TSize idx, const T& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }
    void insert(TSize idx, T&& value) {
        this->_get_space_to_insert(idx) = value;
        ++this->_size;
    }

    virtual T pop(TSize idx, bool decrease_space = true) {
        T res = std::move((*this)[idx]);
        --this->_size;
        for (TSize pos = idx; pos < this->_size; ++pos) {
            this->_sections[pos / SECTION_SIZE][pos % SECTION_SIZE] = std::move(this->_sections[(pos + 1) / SECTION_SIZE][(pos + 1) % SECTION_SIZE]);
        }
        if (this->_allocated - this->_size < SIZE_INCREMENTOR) {
            this->resize(this->_allocated - (this->_allocated - this->_size) / SIZE_INCREMENTOR * SIZE_INCREMENTOR);
        }
        return res;
    }
    T pop(TSize idx) {
        this->pop(idx, true);
    }

    virtual void remove(TSize idx, bool decrease_space = true) {
        this->pop(idx, decrease_space);
    }

    SectionedVectorIterator<T, TSize> iterator(TSize idx = 0) const {
        return SectionedVectorIterator<T, TSize>(this, idx);
    };
    SectionedVectorIterator<T, TSize> iterator_from_end() const {
        return SectionedVectorIterator<T, TSize>(this, this->_size - 1);
    };
};



template <class T, class TSize = std::size_t>
class SectionedVectorIterator : public ABCContainerIterator {
protected:
    SectionedVector<T, Tsize>* _vector = nullptr;
    TSize _idx = 0;
public:
    SectionedVectorIterator() = delete;
    SectionedVectorIterator(SectionedVector<T, Tsize>* vector, TSize idx = 0) : _vector(vector), _idx(idx), ABCContainerIterator(idx >= vector->get_size() || idx < 0) { };
    SectionedVectorIterator(SectionedVector<T, Tsize>& vector, TSize idx = 0) : SectionedVectorIterator(&vector, idx) { }
    SectionedVectorIterator(const SectionedVectorIterator<T, TSize>& other) : _vector(other._vector), _idx(other._idx), ABCContainerIterator(other._stopped) { }

    SectionedVectorIterator<T, TSize>& operator=(const SectionedVectorIterator<T, TSize>& other) {
        this->_vector = other._vector;
        this->_idx = other._idx;
        this->_stopped = other._stopped;
    }

    // movement constructor and assignment operator method is such as copying ones

    T& operator*() {
        if (this->_stopped) {
            throw ValueError("Iterator is stopped");
        }
        return this->_vector[idx];
    }

    SectionedVectorIterator<T, TSize>& operator++() {
        ++this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return *this;
    };
    SectionedVectorIterator<T, TSize> operator++(int) {
        SectionedVectorIterator<T, TSize> old = &this;
        ++this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return old;
    };
    SectionedVectorIterator<T, TSize>& operator--() {
        --this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return *this;
    };
    SectionedVectorIterator<T, TSize> operator--(int) {
        SectionedVectorIterator<T, TSize> old = &this;
        --this->_idx;
        this->_stopped = (this->idx >= this->_vector->get_size() || this->_idx < 0);
        return old;
    };
};