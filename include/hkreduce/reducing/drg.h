#pragma once

#include <stdexcept>

#include "../adjacency_matrix/abc.h"
#include "../collections/array_based.h"
#include "../allocators/wrappers.h"

using namespace std;

template <class TCoef>
class DRG{
private:
    typedef vector<bool, WrapperOfNewStyleAllocator<char>> BoolVector;

    void removeEdges(ABCAdjacencyMatrix<TCoef>& matrix, TCoef threshold, Allocator* allocator) const {
        NeighboursIterator<TCoef> iterator;
        for(size_t from = 0; from < matrix.getSize(); ++from){
            matrix.replaceNeighboursIterator(from, 0, iterator, allocator);
            for (; !iterator.getStopped(); ++iterator) {
                if (iterator.getCoef() < threshold) {
                    iterator.setCoef(0);
                }
            }
        }
    }

    class AllocatorForInnerIterators: public Allocator {
    private:
        Allocator* originalAllocator;
        void* space;
        BoolVector freeSpaceBitmap;
        size_t maxStackSize;

    public:
        AllocatorForInnerIterators(Allocator* allocator, size_t maxStackSize): 
            originalAllocator(allocator),
            space(nullptr),
            freeSpaceBitmap(maxStackSize, true, WrapperOfNewStyleAllocator<char>(allocator)),
            maxStackSize(maxStackSize)
            {}

        void* allocate(size_t innerIteratorSize) override {
            if(this->space == nullptr){
                this->space = this->originalAllocator->allocate(innerIteratorSize * this->maxStackSize);
            }

            for(size_t idx = 0; idx < this->maxStackSize; ++idx){
                if(this->freeSpaceBitmap[idx]){
                    this->freeSpaceBitmap[idx] = false;
                    return (void*) ((char*) (this->space) + (idx * innerIteratorSize));
                }
            }
            throw bad_alloc("Attempt to allocate (n + 1)th allocator");
        }

        void deallocate(void* ptr) override {
            size_t idx = (size_t) ((char*) ptr - (char*) this->space);             
            this->freeSpaceBitmap[idx] = true;
        }

        ~AllocatorForInnerIterators(){
            if(this->space != nullptr){
                this->originalAllocator->deallocate(this->space);
            }
        }
    };

    void checkAchievables(ABCAdjacencyMatrix<TCoef>& matrix, size_t source, BoolVector& achievables, Allocator* allocator) const {
        using Pair = pair<size_t, NeighboursIterator<TCoef>>;
        Pair* stackArray = allocator->allocate(sizeof(Pair) * matrix.getSize());
        ArrayCollection<Pair> stack(stackArray, matrix.getSize(), 0, true, allocator);

        AllocatorForInnerIterators allocatorForInnerIterators(allocator, matrix.getSize());
        
        stack.append(Pair(source, matrix.getNeighboursIterator(source, 0, allocatorForInnerIterators)));
        achievables[source] = true;

        while (stack.getSize() != 0) {
            Pair& pair = stack[stack.size() - 1];

            bool added = false;
            for (NeighboursIterator<TCoef>& iterator = pair.second; !iterator.getStopped(); ++iterator) {
                size_t neighbour = iterator.to();
                if (achievables[neighbour]) {
                    continue;
                }
                achievables[neighbour] = true;
                stack.append(Pair(neighbour, matrix.getNeighboursIterator(neighbour, 0, allocatorForInnerIterators)));
                added = true;
                break;
            }
            if (!added) {
                stack.remove(stack.size() - 1);
            }
        }
    }

public:
    BoolVector run(
        ABCAdjacencyMatrix<TCoef>& matrix, 
        const std::vector<std::size_t>& sources, 
        TCoef threshold, Allocator* allocator
    ) const {
        this->_remove_edges(matrix, threshold);

        BoolVector achievables(matrix.size(), false);

        typedef typename std::vector<std::size_t>::const_iterator Iterator;
        for (Iterator iterator = sources.cbegin(); iterator != sources.cend(); ++iterator) {
            std::size_t source = *iterator;
            if (source > matrix->size()) {
                throw std::invalid_argument("Index of source is out of range");
            }
            if (achievables[source]) {
                continue;
            }
            this->_check_achievables(matrix, source, achievables);
        }
        return achievables;
    };
};