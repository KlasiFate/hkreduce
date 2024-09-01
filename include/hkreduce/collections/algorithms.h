#pragma once

#include <functional>

#include "../collections/array_based.h"
#include "../collections/sectioned.h"


using namespace std;


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeft(
    TCollection* collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    if (stop == SIZE_MAX) {
        stop = collection->getSize();
    }

    if (start > collection->getSize() || stop > collection->getSize()) {
        throw out_of_range("Start or stop argument is out of range");
    }
    if (start > stop) {
        throw invalid_argument("Start is greater than stop");
    }

    const TCollection& collectionRebound = *collection;
    while (stop - start > 0) {
        size_t middle = (stop + start) / 2;

        const T& middleElement = collectionRebound[middle];

        // compare is function that equals "middleElement <= element" expression
        if (compare(middleElement, element)) {
            start = middle + 1;
        }
        else {
            stop = middle;
        }
    }

    return start;
};

template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeft(
    TCollection* collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeft<T, TCollection>(
        collection,
        element,
        [] (const T& middleElement, const T& element) {return middleElement <= element;},
        start,
        stop
    );
};


template<class T>
size_t bsearchLeft(
    ArrayCollection<T>* collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
){
    return bsearchLeft<T, ArrayCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchLeft(
    ArrayCollection<T>* collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
){
    return bsearchLeft<T, ArrayCollection<T>>(collection, element, start, stop);
};

template<class T>
size_t bsearchLeft(
    SectionedCollection<T>* collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
){
    return bsearchLeft<T, SectionedCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchLeft(
    SectionedCollection<T>* collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
){
    return bsearchLeft<T, SectionedCollection<T>>(collection, element, start, stop);
};