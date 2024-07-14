#pragma once
#include "vector"
#include "stdlib.h"

#include "../adjacency_matrix/abc.h"
#include "../collections/array_based.h"


template<class TCoef>
class DRGEP {
private:
    void _insert(BasedOnFixedArrayCollection<TCoef>& paths_lengths, std::size node) {
        TCoef path_length = paths_lengths[node];
        std::size_t idx = queue->size();
        // allocate space
        queue.append(0);
        for (; idx > 0; --idx) {
            if (path_length <= paths_lengths[queue[idx - 1]]) {
                break;
            }
            else {
                queue[idx] = queue[idx - 1];
            }
        }
        queue[idx] = node;
    }

    void _update(){}

    std::vector<TCoef> _run_and_return_paths(ABCAdjacencyMatrix<TCoef>& matrix, std::size_t from, TCoef threshold) const {
        std::size_t* queue_array = new std::size_t[matrix.size()];
        BasedOnFixedArrayCollection<std::size_t> ordered_queue(queue_array, matrix.size(), 0, true);

        TCoef* paths_lengths_array = new TCoef[matrix.size()];
        BasedOnFixedArrayCollection<TCoef> paths_lengths(paths_lengths_array, matrix.size(), matrix.size(), true);

        ordered_queue.append(from);
        paths_lengths[from] = 1;

        NeighboursIterator<TCoef> iterator;
        while(ordered_queue.size()){
            std::size_t current_node = ordered_queue.remove(ordered_queue.size() - 1);
            TCoef current_path_length = paths_lengths[current_node];

            matrix.neighbours_iterator(current_node, 0, iterator);
            // queue_changed = false;
            for(; !iterator.stopped(); ++iterator){
                std::size_t neighbour = iterator.to();
                TCoef neighbour_path_length = iterator.coef() * current_path_length;
                if(neighbour_path_length <= paths_lengths[neighbour]){
                    continue;
                }
                if(neighbour_path_length < threshold){
                    continue;
                }
                if(paths_lengths[neighbour] == 0){
                    // add to queue using binary search
                }
                paths_lengths[neighbour] = neighbour_path_length;
            }

        }
    }


};