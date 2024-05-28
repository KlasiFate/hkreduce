#pragma once

#include <vector>
#include <functional>

#include "adjacency_matrix/abc.h"
#include "adjacency_matrix/csr.h"
#include "_sectioned_vector.h"
#include "errors.h"


template<class TCoef = float>
class RemoveEdges {
public:
    void run(ABCAdjacencyMatrix<TCoef>* matrix, TCoef threshold) const {
        for (EdgeIterator<TCoef> iterator = matrix->iterator(0, 0, true); !iterator.stopped(); ++iterator) {
            TCoef coef = iterator.coef();
            if (coef < threshold) {
                matrix->set(iterator.from(), iterator.to(), 0);
            }
        }
    }

    void run(ABCAdjacencyMatrix<TCoef>& matrix, TCoef threshold) const {
        this->run(&matrix, threshold);
    }

    void run(const ABCAdjacencyMatrix<TCoef>* from, ABCAdjacencyMatrix<TCoef>* to, TCoef threshold) const {
        if (from->size() != to->size()) {
            throw ValueError<>("Matrixes have different sizes");
        }
        for (_ABCEdgeIterator<TCoef> iterator = from->iterator(0, 0, true); !iterator.stopped(); ++iterator) {
            TCoef coef = iterator.coef();
            if (coef >= threshold) {
                to->set(iterator.from(), iterator.to(), coef);
            }
        }
    }

    void run(const ABCAdjacencyMatrix<TCoef>& from, ABCAdjacencyMatrix<TCoef>& to, TCoef threshold) const {
        this->run(&from, &to, threshold);
    }
};


template<class TCoef = float>
class CheckAchievables {
protected:
    std::size_t _section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE;

public:
    CheckAchievables(std::size_t stack_section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE) : _section_size(stack_section_size) {
        if (stack_section_size < 1) {
            throw ValueError<>("Stack section size argument must be greater than 1");
        }
    };

    void run(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t source, std::vector<bool>* achievables) const {
        if (matrix->size() != achievables->size()) {
            throw ValueError<>("Matrixes and achievables vector have different sizes");
        }
        if (source >= matrix->size()) {
            throw ValueError<>("Index is out of range");
        }

        using Pair = std::pair<std::size_t, NeighboursIterator<TCoef>>;

        _SectionedVector<Pair> stack(this->_section_size);
        stack.append(Pair(source, matrix->neighbours_iterator(source)));
        (*achievables)[source] = true;

        while (stack.size() != 0) {
            Pair& pair = stack[stack.size() - 1];

            bool added = false;
            for (NeighboursIterator<TCoef>& iterator = pair.second; !iterator.stopped(); ++iterator) {
                std::size_t neighbour = iterator.neighbour();
                if ((*achievables)[neighbour]) {
                    continue;
                }
                (*achievables)[neighbour] = true;
                stack.append(Pair(neighbour, matrix->neighbours_iterator(neighbour)));
                added = true;
                break;
            }
            if (!added) {
                stack.pop(stack.size() - 1);
            }
        }
    }

    void run(const ABCAdjacencyMatrix<TCoef>& matrix, std::size_t source, std::vector<bool>& achievables) const {
        this->run(&matrix, source, &achievables);
    }

    std::vector<bool> run(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t source) const {
        std::vector<bool> achievables(matrix->size(), false);
        this->run(&matrix, source, &achievables);
        return achievables;
    }

    std::vector<bool> run(const ABCAdjacencyMatrix<TCoef>& matrix, std::size_t source) const {
        return this->run(&matrix, source);
    }
};


template<class TCoef = float>
class SearchLengthPath {
public:
    using Accumulate = std::function<TCoef(TCoef, TCoef)>;
    using Compare = std::function<bool(TCoef, TCoef)>;
    using Limit = std::function<bool(TCoef)>;

    using AllPathsLengths = std::pair<std::vector<TCoef>, std::vector<bool>>;
    using Achievables = std::vector<bool>;
protected:
    std::size_t _section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE;
    Accumulate _accumulate = nullptr;
    Compare _compare = nullptr;
    TCoef _initial_path_value = 0;
    Limit _limit = nullptr;
public:

    SearchLengthPath(
        Accumulate accumulate = [] (TCoef length, TCoef coef) -> TCoef {return length + coef;},
        TCoef initial_path_value = 0,
        Compare compare_paths_lengths = [] (TCoef min_coef, TCoef coef) -> TCoef {return min_coef <= coef;},
        Limit limit_path = nullptr,
        std::size_t stack_section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE
    ) : _section_size(stack_section_size), _accumulate(accumulate), _compare(compare_paths_lengths), _initial_path_value(initial_path_value), _limit(limit_path) {
        if (stack_section_size < 1) {
            throw ValueError<>("Stack section size argument must be greater than 1");
        }
    };

    AllPathsLengths run_and_return_paths(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from) const {
        std::vector<TCoef> paths_lengths(matrix->size(), this->_initial_path_value);
        std::vector<bool> not_infinite_paths(matrix->size(), false);
        not_infinite_paths[from] = true;

        _SectionedVector<std::size_t> to_see(this->_section_size);
        std::function<void(std::size_t)> insert = [&to_see, &paths_lengths] (std::size_t node) -> void {
            TCoef path_length = paths_lengths[node];
            std::size_t idx = 0;
            for (; idx < to_see.size(); ++idx) {
                if (path_length < paths_lengths[to_see[idx]]) {
                    break;
                }
            }
            to_see.insert(idx, node);
        };
        std::function<std::size_t()> pop_first = [&to_see] () -> std::size_t {
            return to_see.pop(0);
        };
        insert(from);

        while (to_see.size()) {
            std::size_t current_node = pop_first();
            TCoef curent_path_length = paths_lengths[current_node];

            for (NeighboursIterator<TCoef> iterator = matrix->neighbours_iterator(current_node); !iterator.stopped(); ++iterator) {
                std::size_t next_node = iterator.neighbour();
                TCoef next_path_length = this->_accumulate(curent_path_length, iterator.coef());

                if (this->_limit != nullptr && this->_limit(next_path_length)) {
                    continue;
                }
                if (this->_compare(paths_lengths[next_node], next_path_length) && not_infinite_paths[next_node]) {
                    continue;
                }

                paths_lengths[next_node] = next_path_length;
                not_infinite_paths[next_node] = true;

                insert(next_node);
            }
        }

        return AllPathsLengths(std::move(paths_lengths), std::move(not_infinite_paths));
    }

    AllPathsLengths run_and_return_paths(const ABCAdjacencyMatrix<TCoef>& matrix, std::size_t from) const {
        return this->run_and_return_paths(&matrix, from);
    }

    Achievables run_and_return_achievables(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from) const {
        return this->run_and_return_paths(matrix, from).second;
    }
    
    Achievables run_and_return_achievables(const ABCAdjacencyMatrix<TCoef>& matrix, std::size_t from) const {
        return this->run_and_return_achievables(&matrix, from);
    }

    TCoef run(const ABCAdjacencyMatrix<TCoef>* matrix, std::size_t from, std::size_t to) const {
        AllPathsLengths result = this->run_and_return_paths(matrix, from);
        if (!result.second[to]) {
            throw PathNotFoundError<>("No path found to a node provided by \"to\" argument.");
        }
        return result.first[to];
    }

    TCoef run(const ABCAdjacencyMatrix<TCoef>& matrix, std::size_t from, std::size_t to) const {
        return this->run(&matrix, from, to);
    }
};


template<class TCoef = float>
class DRGAndPFA {
protected:
    std::size_t _section_size;
public:
    DRGAndPFA(std::size_t section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE) : _section_size(section_size) { };

    std::vector<bool> run(ABCAdjacencyMatrix<TCoef>* matrix, const std::vector<std::size_t>* sources, TCoef threshold) const {
        RemoveEdges<TCoef> remove_edges_alg = RemoveEdges<TCoef>();
        remove_edges_alg.run(matrix, threshold);

        CheckAchievables<TCoef> check_achievables_alg(this->_section_size);
        std::vector<bool> achievables(matrix->size(), false);

        typedef typename std::vector<std::size_t>::const_iterator Iterator;
        for (Iterator iterator = sources->cbegin(); iterator != sources->cend(); ++iterator) {
            std::size_t source = *iterator;
            if (source > matrix->size()) {
                throw ValueError<>("Index of source is out of range");
            }
            if (achievables[source]) {
                continue;
            }
            check_achievables_alg.run(matrix, source, &achievables);
        }
        return achievables;
    };

    std::vector<bool> run(ABCAdjacencyMatrix<TCoef>& matrix, const std::vector<std::size_t>& sources, TCoef threshold) const {
        return this->run(&matrix, &sources, threshold);
    };
};


template<class TCoef = float>
class DRGEP {
protected:
    std::size_t _section_size;
public:
    DRGEP(std::size_t section_size = _SECTIONED_VECTOR_DEFAULT_SECTION_SIZE) : _section_size(section_size) { };


    std::vector<bool> run(const ABCAdjacencyMatrix<TCoef>* matrix, const std::vector<std::size_t>* sources, TCoef threshold) const {
        std::function<TCoef(TCoef, TCoef)> accumulate = [] (TCoef path_length, TCoef edge_coef) -> TCoef {
            return path_length * edge_coef;
        };

        std::function<bool(TCoef, TCoef)> compare = [] (TCoef max_coef, TCoef coef) -> bool {
            return max_coef >= coef;
        };

        std::function<bool(TCoef)> limit = [threshold] (TCoef path_length) -> bool {
            return path_length < threshold;
        };

        SearchLengthPath<TCoef> search_paths_alg(accumulate, 1, compare, limit, this->_section_size);
        
        std::vector<bool> achievables(matrix->size(), false);
        typedef typename std::vector<std::size_t>::const_iterator Iterator;

        for (Iterator iterator = sources->cbegin(); iterator != sources->cend(); ++iterator){
            std::size_t source = *iterator;
            if (source > matrix->size()) {
                throw ValueError<>("Index of source is out of range");
            }

            typedef typename SearchLengthPath<TCoef>::Achievables Achievables;
            Achievables achievables_paths = search_paths_alg.run_and_return_achievables(matrix, source);

            for(std::size_t idx = 0; idx<matrix->size(); ++idx){
                if(achievables_paths[idx]){
                    achievables[idx] = true;
                }
            }
        }

        return achievables;
    }

    std::vector<bool> run(const ABCAdjacencyMatrix<TCoef>& matrix, const std::vector<std::size_t>& sources, TCoef threshold) const {
        return this->run(&matrix, &sources, threshold);
    };
};

