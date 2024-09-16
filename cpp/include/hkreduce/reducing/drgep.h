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
    void insertToQueue(
        ArrayCollection<size_t>& orderedQueue,
        ArrayCollection<TCoef>& pathsLengths,
        size_t node,
        TCoef newNodeLength
    ) const {
        // The second argument of the function below always equals the second "element" argument of the bsearchRightToInsert
        function<bool(const size_t&, const size_t&)> compare = [&pathsLengths, newNodeLength](const size_t& middleElement, const size_t& node) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];

            if(middleElementPath < newNodeLength){
                return true;
            }
            if(middleElementPath > newNodeLength){
                return false;
            }
            if(middleElement <= node){
                return true;
            }
            return false;
        };

        size_t idxToInsert = bsearchRightToInsert<size_t>(
            orderedQueue,
            node,
            compare
        );

        orderedQueue.insert(idxToInsert, node);
    }

    void updateQueue(
        ArrayCollection<size_t>& orderedQueue,
        ArrayCollection<TCoef>& pathsLengths,
        size_t node,
        TCoef newNodeLength
    ) const {
        function<bool(const size_t&, const size_t&)> compareToInsert = [&pathsLengths, newNodeLength](const size_t& middleElement, const size_t& node) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];

            if(middleElementPath < newNodeLength){
                return true;
            }
            if(middleElementPath > newNodeLength){
                return false;
            }
            if(middleElement <= node){
                return true;
            }
            return false;
        };

        size_t idxToInsert = bsearchRightToInsert<size_t>(
            orderedQueue,
            node,
            compareToInsert
        );

        TCoef nodeLength = pathsLengths[node];
        function<int(const size_t&, const size_t&)> compareToSearch = [&pathsLengths, nodeLength](const size_t& middleElement, const size_t& node) -> int {
            TCoef middleElementPath = pathsLengths[middleElement];

            if(middleElementPath < nodeLength){
                return 1;
            }
            if(middleElementPath > nodeLength){
                return -1;
            }
            if(middleElement < node){
                return 1;
            }
            if(middleElement > node){
                return -1;
            }
            return 0;
        };

        size_t currentIdx = bsearchRight<size_t>(
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
                TCoef newNeighbourPathLength = iterator.getCoef() * currentPathLength;
                if(newNeighbourPathLength <= pathsLengths[neighbour]){
                    continue;
                }
                if(newNeighbourPathLength < threshold){
                    continue;
                }

                TCoef oldNeighbourPathLength = pathsLengths[neighbour];
                if(oldNeighbourPathLength == 0){
                    this->insertToQueue(orderedQueue, pathsLengths, neighbour, newNeighbourPathLength);
                }else{
                    this->updateQueue(orderedQueue, pathsLengths, neighbour, newNeighbourPathLength);
                }
                pathsLengths.replace(neighbour, newNeighbourPathLength);
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
                memset((void*) pathsLengths.getArray(), 0, matrix.getSize() * sizeof(TCoef));
            }
        }

        return result;
    }
};