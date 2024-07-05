#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>

#include "algorithms.h"


int main2(int args_count, char** args) {
    std::ifstream input("test-input.txt");

    std::size_t buf_size = 1024 * 1024;
    char* buf = new char[buf_size];

    input.rdbuf()->pubsetbuf(buf, buf_size);

    // const std::size_t nodes_count = 685230;
    const std::size_t nodes_count = 4;
    // const std::size_t edges_count = 7600595;
    const std::size_t edges_count = 5;

    std::cout << "Creating graph" << std::endl;

    CSRAdjacencyMatrix<std::size_t> matrix(nodes_count, edges_count);
    for (std::size_t idx = 0; idx < edges_count; ++idx) {
        std::size_t from;
        std::size_t to;
        std::size_t coef;

        input >> from >> to >> coef;

        matrix.set(from, to, coef);
    }

    std::function<std::size_t(std::size_t, std::size_t)> accumulate = [](std::size_t first, std::size_t second) -> std::size_t {return first + second;};
    std::function<bool(std::size_t, std::size_t)> compare = [](std::size_t min_coef, std::size_t coef) -> bool {return min_coef <= coef;};

    SearchLengthPath<std::size_t> alg(accumulate, 0, compare);

    std::cout << "Finding paths lengths" << std::endl;

    std::cout << alg.run(matrix, 0, 3) << std::endl;

    return 0;
};







int main(int args_count, char** args) {
    if (args_count == 1) {
        return 1;
    }
    std::string method(args[1]);

    std::size_t sources_count;
    double threshold;

    std::cin >> threshold >> sources_count;
    std::vector<std::size_t> sources(sources_count, 0);
    for (std::size_t idx = 0; idx < sources_count; ++idx) {
        std::size_t source;
        std::cin >> source;
        sources[idx] = source;
    }

    std::size_t size, edges_count;
    std::cin >> size >> edges_count;
    CSRAdjacencyMatrix<double> matrix(size, edges_count);
    for (std::size_t idx = 0; idx < edges_count; ++idx) {
        std::size_t from, to;
        float coef;
        std::cin >> from >> to >> coef;
        if (from >= size || to >= size) {
            throw ValueError<>("Index is out of range");
        }
        if (coef != 0) {
            matrix.set(from, to, coef);
        }
    }

    std::vector<bool> achievables(0, false);
    if (method != "drgep") {
        DRGAndPFA<double> alg;
        achievables = alg.run(matrix, sources, threshold);
    }
    else {
        DRGEP<double> alg;
        achievables = alg.run(matrix, sources, threshold);
    }

    for (std::size_t idx = 0; idx < size; ++idx) {
        if (achievables[idx]) {
            std::cout << idx << " ";
        }
    }
    std::cout << std::endl;

    return 0;
};