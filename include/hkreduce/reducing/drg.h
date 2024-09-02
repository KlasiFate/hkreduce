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
        AllocatorForInnerIterators(Allocator* allocator, size_t maxStackSize): 
            originalAllocator(allocator),
            space(nullptr),
            freeSpaceBitmap(maxStackSize, true, allocator),
            maxStackSize(maxStackSize)
            {}

        void* allocate(size_t innerIteratorSize) override {
            if(this->space == nullptr){
                this->space = (char*) this->originalAllocator->allocate(innerIteratorSize * this->maxStackSize);
                this->innerIteratorSize = innerIteratorSize;
            }else if(this->innerIteratorSize != innerIteratorSize){
                return this->originalAllocator->allocate(innerIteratorSize);
            }

            for(size_t idx = 0; idx < this->maxStackSize; ++idx){
                BoolReference value = this->freeSpaceBitmap[idx]
                if(value){
                    value = false;
                    return (void*) (this->space + (idx * innerIteratorSize));
                }
            }
            return this->originalAllocator->allocate(innerIteratorSize);
        }

        void deallocate(void* ptr) override {
            ssize_t idx = (ssize_t) ((char*) ptr - this->space);  
            if(idx < 0 || idx >= this->maxStackSize * this->innerIteratorSize){
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

    void checkAchievables(ABCAdjacencyMatrix<TCoef>& matrix, size_t source, Bitmap& achievables, Allocator* allocator) const {
        using Pair = pair<size_t, NeighboursIterator<TCoef>>;
        Pair* stackArray = allocator->allocate(sizeof(Pair) * matrix.getSize());
        ArrayCollection<Pair> stack(stackArray, matrix.getSize(), 0, true, allocator);

        AllocatorForInnerIterators allocatorForInnerIterators(allocator, matrix.getSize());
        
        stack.append(Pair(source, matrix.getNeighboursIterator(source, 0, allocatorForInnerIterators)));
        achievables[source] = true;

        while (stack.getSize() > 0) {
            Pair& pair = stack[stack.getSize() - 1];

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
    Bitmap run(
        ABCAdjacencyMatrix<TCoef>& matrix, 
        IndexableCollection<size_t>& sources, 
        TCoef threshold, 
        Allocator* allocator
    ) const {
        this->removeEdges(matrix, threshold);

        Bitmap achievables(matrix.getSize(), false);

        for (size_t i = 0; i < sources.getSize(); ++i) {
            source = sources[i];
            if (source > matrix->size()) {
                throw invalid_argument("Index of source is out of range");
            }
            if (achievables[source]) {
                continue;
            }
            this->checkAchievables(matrix, source, achievables);
        }
        return achievables;
    };
};