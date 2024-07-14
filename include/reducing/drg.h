#pragma once

#include <stdexcept>

#include "adjacency_matrix/abc.h"
#include "collections/array_based.h"


template <class TCoef>
class DRG{
private:
    void _remove_edges(ABCAdjacencyMatrix<TCoef>& matrix, TCoef threshold) const {
        NeighboursIterator<TCoef> iterator;
        for(std::size_t from = 0; from < matrix.size(); ++from){
            matrix.neighbours_iterator(from, 0, iterator);
            for (; !iterator.stopped(); ++iterator) {
                if (iterator.coef() < threshold) {
                    iterator->set(0);
                }
            }
        }
    }

    void _check_achievables(ABCAdjacencyMatrix<TCoef>& matrix, std::size_t source, std::vector<bool>& achievables) const {
        using Pair = std::pair<std::size_t, NeighboursIterator<TCoef>>;
        Pair* stack_array = new Pair[matrix.size()];
        BasedOnFixedArrayCollection<Pair> stack(stack_array, matrix.size(), 0, true);

        stack.append(Pair(source, matrix->neighbours_iterator(source, 0)));
        achievables[source] = true;

        while (stack.size() != 0) {
            Pair& pair = stack[stack.size() - 1];

            bool added = false;
            for (NeighboursIterator<TCoef>& iterator = pair.second; !iterator.stopped(); ++iterator) {
                std::size_t neighbour = iterator.to();
                if (achievables[neighbour]) {
                    continue;
                }
                achievables[neighbour] = true;
                stack.append(Pair(neighbour, matrix.neighbours_iterator(neighbour, 0)));
                added = true;
                break;
            }
            if (!added) {
                stack.remove(stack.size() - 1);
            }
        }
    }

public:
    std::vector<bool> run(ABCAdjacencyMatrix<TCoef>& matrix, const std::vector<std::size_t>& sources, TCoef threshold) const {
        this->_remove_edges(matrix, threshold);

        std::vector<bool> achievables(matrix.size(), false);

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