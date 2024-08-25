#pragma once

#include <functional>

#include "../collections/abc.h"


using namespace std;


// Для возможности специализации, при которой компилятор сможет сделать inline
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
        [] (const T& middleElement, const T& element) {return middleElement <= element},
        start,
        stop
    )
}