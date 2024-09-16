#pragma once

#include <functional>

#include "../collections/array_based.h"
#include "../collections/sectioned.h"
#include "../collections/bitmap.h"


using namespace std;


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchRightToInsert(
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
        }
        else {
            stop = middle;
        }
    }

    return start;
};

template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchRightToInsert(
    const TCollection& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRightToInsert<T, TCollection>(
        collection,
        element,
        [] (const T& middleElement, const T& element) {return middleElement <= element;},
        start,
        stop
    );
};


template<class T>
size_t bsearchRightToInsert(
    const ArrayCollection<T>& collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRightToInsert<T, ArrayCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchRightToInsert(
    const ArrayCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRightToInsert<T, ArrayCollection<T>>(collection, element, start, stop);
};

template<class T>
size_t bsearchRightToInsert(
    const SectionedCollection<T>& collection,
    const T& element,
    function<bool(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRightToInsert<T, SectionedCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchRightToInsert(
    const SectionedCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRightToInsert<T, SectionedCollection<T>>(collection, element, start, stop);
};


template<class T, class TCollection = IndexableCollection<T>>
size_t bsearchRight(
    const TCollection& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    function<bool(const T&, const T&)> compare2 = [&compare] (const T& middleElement, const T& element) -> int {
        return 0 <= compare(middleElement, element);
        };

    size_t idxToInsert = bsearchRightToInsert<T, TCollection>(collection, element, compare2, start, stop);

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
size_t bsearchRight(
    const TCollection& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    function<int(const T&, const T&)> compare = [] (const T& first, const T& second) -> int {
        return first == second ? 0 : (first < second ? -1 : 1);
        };
    return bsearchRight<T, TCollection>(
        collection,
        element,
        compare,
        start,
        stop
    );
}


template<class T>
size_t bsearchRight(
    const ArrayCollection<T>& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRight<T, ArrayCollection<T>>(collection, element, compare, start, stop);
};


template<class T>
size_t bsearchRight(
    const SectionedCollection<T>& collection,
    const T& element,
    function<int(const T&, const T&)> compare,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRight<T, SectionedCollection<T>>(collection, element, compare, start, stop);
};

template<class T>
size_t bsearchRight(
    const ArrayCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRight<T, ArrayCollection<T>>(collection, element, start, stop);
};


template<class T>
size_t bsearchRight(
    const SectionedCollection<T>& collection,
    const T& element,
    size_t start = 0,
    size_t stop = SIZE_MAX
) {
    return bsearchRight<T, SectionedCollection<T>>(collection, element, start, stop);
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

    size_t fullFilledSectionsCount = collection.getSize() / Bitmap::BoolSection::BITS_COUNT_IN_SECTION;
    const IndexableCollection<Bitmap::BoolSection>* sections = collection.getBoolSections();
    for (size_t i = 0; i < fullFilledSectionsCount; ++i) {
        Bitmap::BoolSection section = (*sections)[i];
        result += section.countBits();
    }

    for (size_t i = fullFilledSectionsCount * Bitmap::BoolSection::BITS_COUNT_IN_SECTION; i < collection.getSize(); ++i) {
        if (collection[i]) {
            ++result;
        }
    }

    if (!value) {
        result = collection.getSize() - result;
    }
    return result;
};