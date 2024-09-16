#pragma once

#include <stdexcept>

#include "../adjacency_matrix/abc.h"
#include "../collections/array_based.h"
#include "../collections/bitmap.h"
#include "../allocators/wrappers.h"

using namespace std;

template <class TCoef>
class DRG{
private:

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
        char* space;
        Bitmap freeSpaceBitmap;
        size_t maxStackSize;
        size_t innerIteratorSize;

    public:
        // + 10 is necessary for copy\move assignment
        AllocatorForInnerIterators(Allocator* allocator, size_t maxStackSize): 
            originalAllocator(allocator),
            space(nullptr),
            freeSpaceBitmap(maxStackSize + 10, true, allocator),
            maxStackSize(maxStackSize + 10),
            innerIteratorSize(0)
            {}

        void* allocate(size_t innerIteratorSize) override {
            if(this->space == nullptr){
                this->innerIteratorSize = innerIteratorSize;
                this->space = (char*) this->originalAllocator->allocate(this->innerIteratorSize * this->maxStackSize);
            }else if(this->innerIteratorSize != innerIteratorSize){
                return this->originalAllocator->allocate(innerIteratorSize);
            }

            for(size_t idx = 0; idx < this->maxStackSize; ++idx){
                BoolReference value = this->freeSpaceBitmap[idx];
                if(value){
                    value = false;
                    return (void*) (this->space + (idx * this->innerIteratorSize));
                }
            }
            return this->originalAllocator->allocate(innerIteratorSize);
        }

        void deallocate(void* ptr) noexcept override {
            ssize_t idx = (ssize_t) ((char*) ptr - this->space);  
            if(idx < 0 || idx >= (ssize_t)(this->maxStackSize * this->innerIteratorSize)){
                this->originalAllocator->deallocate(ptr);
                return;
            }
            this->freeSpaceBitmap[idx / this->innerIteratorSize] = true;
        }

        ~AllocatorForInnerIterators(){
            if(this->space != nullptr){
                this->originalAllocator->deallocate(this->space);
            }
        }
    };

    using Pair = pair<size_t, NeighboursIterator<TCoef>>;

    void checkAchievables(
        ABCAdjacencyMatrix<TCoef>& matrix,
        size_t source,
        Bitmap& achievables,
        ArrayCollection<Pair>& stack,
        AllocatorForInnerIterators* allocatorForInnerIterators
    ) const {
        stack.append(Pair(source, matrix.getNeighboursIterator(source, 0, allocatorForInnerIterators)));
        achievables[source] = true;

        while (stack.getSize() > 0) {
            Pair& pair = stack[stack.getSize() - 1];

            bool added = false;
            for (NeighboursIterator<TCoef>& iterator = pair.second; !iterator.getStopped(); ++iterator) {
                size_t neighbour = iterator.getTo();
                BoolReference isAchievable = achievables[neighbour];
                if (isAchievable) {
                    continue;
                }
                isAchievable = true;
                stack.append(Pair(neighbour, matrix.getNeighboursIterator(neighbour, 0, allocatorForInnerIterators)));
                added = true;
                break;
            }
            if (!added) {
                stack.remove(stack.getSize() - 1);
            }
        }
    }

public:
    Bitmap run(
        ABCAdjacencyMatrix<TCoef>& matrix, 
        IndexableCollection<size_t>& sources, 
        TCoef threshold, 
        Allocator* allocator
    ) const {
        this->removeEdges(matrix, threshold, allocator);

        Bitmap achievables(matrix.getSize(), false);

        AllocatorForInnerIterators allocatorForInnerIterators(allocator, matrix.getSize());
        
        ArrayCollection<Pair> stack(matrix.getSize(), allocator);

        for (size_t i = 0; i < sources.getSize(); ++i) {
            size_t source = sources[i];
            if (source > matrix.getSize()) {
                throw invalid_argument("Index of source is out of range");
            }
            if (achievables[source]) {
                continue;
            }
            this->checkAchievables(matrix, source, achievables, stack, &allocatorForInnerIterators);
        }
        return achievables;
    };
};