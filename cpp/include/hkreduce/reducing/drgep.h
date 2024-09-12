#pragma once
// #include <cstdlib>
#include <cstring>

#include "../adjacency_matrix/abc.h"
#include "../collections/array_based.h"
#include "../collections/algorithms.h"
#include "../collections/bitmap.h"


using namespace std;

template<class TCoef>
class DRGEP {
private:
    void insertToOrderedQueue(
        ArrayCollection<size_t>& orderedQueue,
        ArrayCollection<TCoef>& pathsLengths,
        size_t node
    ) const {
        function<bool(const size_t&, const size_t&)> compare = [&pathsLengths](const size_t& middleElement, const size_t& element) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];
            TCoef elementPath = pathsLengths[element];
            if(middleElementPath < elementPath){
                return true;
            }
            if(middleElementPath > elementPath){
                return false;
            }
            if(middleElement <= element){
                return true;
            }
            return false;
        };

        size_t idxToInsert = bsearchLeftToInsert<size_t>(
            orderedQueue,
            node,
            compare
        );

        orderedQueue.insert(idxToInsert, node);
    }

    void updateQueue(
        ArrayCollection<size_t>& orderedQueue,
        ArrayCollection<TCoef>& pathsLengths,
        size_t node
    ) const {
        function<bool(const size_t&, const size_t&)> compareToInsert = [&pathsLengths](const size_t& middleElement, const size_t& element) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];
            TCoef elementPath = pathsLengths[element];
            if(middleElementPath < elementPath){
                return true;
            }
            if(middleElementPath > elementPath){
                return false;
            }
            if(middleElement <= element){
                return true;
            }
            return false;
        };

        size_t idxToInsert = bsearchLeftToInsert<size_t>(
            orderedQueue,
            node,
            compareToInsert
        );

        if(pathsLengths[node] == 0){
            throw invalid_argument("No provided node in ordered queue");
        }

        function<int(const size_t&, const size_t&)> compareToSearch = [&pathsLengths](const size_t& middleElement, const size_t& element) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];
            TCoef elementPath = pathsLengths[element];
            if(middleElementPath < elementPath){
                return 1;
            }
            if(middleElementPath > elementPath){
                return -1;
            }
            if(middleElement < element){
                return 1;
            }
            if(middleElement > element){
                return -1;
            }
            return 0;
        };

        size_t currentIdx = bsearchLeft<size_t>(
            orderedQueue,
            node,
            compareToSearch
        );     

        if(currentIdx == SIZE_MAX){
            throw invalid_argument("No provided node in ordered queue");
        }   

        if(currentIdx + 1 == idxToInsert){
            return;
        }

        memmove(
            orderedQueue.getArray() + currentIdx,
            orderedQueue.getArray() + currentIdx + 1,
            (idxToInsert - currentIdx - 1) * sizeof(size_t)
        );
        orderedQueue.getArray()[idxToInsert - 1] = node;
    }
    
    void calcPathLengths(
        ABCAdjacencyMatrix<TCoef>& matrix,
        size_t from,
        TCoef threshold,
        ArrayCollection<size_t>& orderedQueue,
        ArrayCollection<TCoef>& pathsLengths,
        Allocator* allocator
    ) const {
        orderedQueue.append(from);
        pathsLengths[from] = 1;

        NeighboursIterator<TCoef> iterator;
        while(orderedQueue.getSize()){
            size_t currentNode = orderedQueue.remove(orderedQueue.getSize() - 1);
            TCoef currentPathLength = pathsLengths[currentNode];

            matrix.replaceNeighboursIterator(currentNode, 0, iterator, allocator);

            for(; !iterator.getStopped(); ++iterator){
                size_t neighbour = iterator.getTo();
                TCoef neighbourPathLength = iterator.getCoef() * currentPathLength;
                if(neighbourPathLength <= pathsLengths[neighbour]){
                    continue;
                }
                if(neighbourPathLength < threshold){
                    continue;
                }
                TCoef old = pathsLengths.replace(neighbour, neighbourPathLength);

                if(old == 0){
                    this->insertToOrderedQueue(orderedQueue, pathsLengths, neighbour);
                }else{
                    this->updateQueue(orderedQueue, pathsLengths, neighbour);
                }
            }
        }
    }

public:
    Bitmap run(
        ABCAdjacencyMatrix<TCoef>& matrix, 
        IndexableCollection<size_t>& sources, 
        TCoef threshold, 
        Allocator* allocator
    ){
        ArrayCollection<size_t> orderedQueue(matrix.getSize(), allocator);
        ArrayCollection<TCoef> pathsLengths(matrix.getSize(), 0, allocator);

        Bitmap result(matrix.getSize(), false, allocator);

        size_t* queueArray = (size_t*) allocator->allocate(matrix.getSize() * sizeof(size_t));
        for(size_t i = 0; i < sources.getSize(); ++i){
            size_t source = sources[i];

            this->calcPathLengths(
                matrix,
                source,
                threshold,
                orderedQueue,
                pathsLengths,
                allocator
            );
            for(size_t i = 0; i < matrix.getSize(); ++i){
                if(pathsLengths[i] >= threshold){
                    result[i] = true;
                }
            }

            if(i + 1 < sources.getSize()){
                orderedQueue.clear();
                memset((void*) pathsLengths.getArray(), 0, matrix.getSize() * sizeof(TCoef));
            }
        }

        return result;
    }
};