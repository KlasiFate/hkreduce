#pragma once

#include <functional>

#include "../collections/array_based.h"
#include "../collections/sectioned.h"


using namespace std;


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeftToInsert(
    const TCollection& collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    if (stop == SIZE_MAX) {
        stop = collection.getSize();
    }

    if (start > collection.getSize() || stop > collection.getSize()) {
        throw out_of_range("Start or stop argument is out of range");
    }
    if (start > stop) {
        throw invalid_argument("Start is greater than stop");
    }

    while (stop - start > 0) {
        size_t middle = (stop + start) / 2;

        const T& middleElement = collection[middle];

        // compare is function that equals "middleElement <= element" expression
        if (compare(middleElement, element)) {
            start = middle + 1;
        }else {
            stop = middle;
        }
    }

    return start;
};

template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeftToInsert(
    const TCollection& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeftToInsert<T, TCollection>(
        collection,
        element,
        [] (const T& middleElement, const T& element) {return middleElement <= element;},
        start,
        stop
    );
};


template<class T>
size_t bsearchLeftToInsert(
    const ArrayCollection<T>& collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeftToInsert<T, ArrayCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchLeftToInsert(
    const ArrayCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeftToInsert<T, ArrayCollection<T>>(collection, element, start, stop);
};

template<class T>
size_t bsearchLeftToInsert(
    const SectionedCollection<T>& collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeftToInsert<T, SectionedCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchLeftToInsert(
    const SectionedCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeftToInsert<T, SectionedCollection<T>>(collection, element, start, stop);
};


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeft(
    const TCollection& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    function<bool(const T&, const T&)> compare2 = [&compare] (const T& middleElement, const T& element) -> int {
        return compare(middleElement, element) <= 0;
        };

    size_t idxToInsert = bsearchLeftToInsert<T, TCollection>(collection, element, compare2, start, stop);

    // not found
    if (idxToInsert == 0) {
        return SIZE_MAX;
    }
    if (compare(collection[idxToInsert - 1], element) == 0) {
        return idxToInsert - 1;
    }
    return SIZE_MAX;
};


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchLeft(
    const TCollection& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    function<int(const T&, const T&)> compare = [] (const T& first, const T& second) -> int {
        return first == second ? 0 : (first < second ? -1 : 1);
        };
    return bsearch<T, TCollection>(
        collection,
        element,
        compare,
        start,
        stop
    );
}


template<class T>
size_t bsearchLeft(
    const ArrayCollection<T>& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeft<T, ArrayCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchLeft(
    const SectionedCollection<T>& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeft<T, SectionedCollection<T>>(collection, element, compare, start, stop);
};

template<class T>
size_t bsearchLeft(
    const ArrayCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeft<T, ArrayCollection<T>>(collection, element, start, stop);
};


template<class T>
size_t bsearchLeft(
    const SectionedCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchLeft<T, SectionedCollection<T>>(collection, element, start, stop);
};


template<class TBoolCollection = IndexableCollection<bool>>
size_t countBits(
    const TBoolCollection& collection,
    const bool value = true
) {
    size_t result = 0;
    for (size_t i = 0; i < collection.getSize(); ++i) {
        if (collection[i] == value) {
            ++result;
        }
    }
    return result;
};

template<>
size_t countBits(
    const Bitmap& collection,
    const bool value
) {
    size_t result = 0;

    size_t sectionsCount = collection.getSize() / Bitmap::BITS_COUNT_IN_SECTION;
    const IndexableCollection<Bitmap::BoolSection>* sections = collection.getBoolSections();
    for (size_t i = 0; i < sectionsCount; ++i) {
        Bitmap::BoolSection section = (*sections)[i];

        for (size_t j = 0; j < Bitmap::BITS_COUNT_IN_SECTION; ++j) {
            if (section & (1 << j)) {
                ++result;
            }
        }
    }

    for (size_t i = sectionsCount * Bitmap::BITS_COUNT_IN_SECTION; i < collection.getSize(); ++i) {
        if (collection[i]) {
            ++result;
        }
    }

    if (!value) {
        result = collection.getSize() - result;
    }
    return result;
};