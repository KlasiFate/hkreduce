#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>

#include "graph_reducing/adjacency_matrix/csr.h"
#include "graph_reducing/collections/array_based.h"

using namespace std;


int main2(int args_count, char** args) {
    ifstream input("test-input.txt");

    size_t buf_size = 1024 * 1024;
    char* buf = new char[buf_size];

    input.rdbuf()->pubsetbuf(buf, buf_size);

    // const std::size_t nodes_count = 685230;
    const size_t nodes_count = 4;
    // const std::size_t edges_count = 7600595;
    const size_t edges_count = 5;

    std::cout << "Creating graph" << std::endl;

    ArrayCollection<size_t> rows(new size_t[nodes_count], nodes_count);
    ArrayCollection<size_t> cols(new size_t[edges_count], edges_count);
    ArrayCollection<double> coefs(new double[edges_count], edges_count);

    for (size_t i = 0; i < edges_count; ++i) {
        size_t from;
        size_t to;
        double coef;

        input >> from >> to >> coef;

        rows[from] += 1;
        cols[i] = to;
        coefs[i] = coef; 
    }

    size_t accumulators = 0;
    for(size_t i = 0; i < nodes_count; ++i){
        size_t copy = rows[i];
        rows[i] += accumulators;
        accumulators += copy;
    }

    CSRAdjacencyMatrix<double> matrix(
        &rows,
        &cols,
        &coefs,
        false
    );

    return 0;
};







// int main(int args_count, char** args) {
//     if (args_count == 1) {
//         return 1;
//     }
//     std::string method(args[1]);

//     std::size_t sources_count;
//     double threshold;

//     std::cin >> threshold >> sources_count;
//     std::vector<std::size_t> sources(sources_count, 0);
//     for (std::size_t idx = 0; idx < sources_count; ++idx) {
//         std::size_t source;
//         std::cin >> source;
//         sources[idx] = source;
//     }

//     std::size_t size, edges_count;
//     std::cin >> size >> edges_count;
//     CSRAdjacencyMatrix<double> matrix(size, edges_count);
//     for (std::size_t idx = 0; idx < edges_count; ++idx) {
//         std::size_t from, to;
//         float coef;
//         std::cin >> from >> to >> coef;
//         if (from >= size || to >= size) {
//             throw ValueError<>("Index is out of range");
//         }
//         if (coef != 0) {
//             matrix.set(from, to, coef);
//         }
//     }

//     std::vector<bool> achievables(0, false);
//     if (method != "drgep") {
//         DRGAndPFA<double> alg;
//         achievables = alg.run(matrix, sources, threshold);
//     }
//     else {
//         DRGEP<double> alg;
//         achievables = alg.run(matrix, sources, threshold);
//     }

//     for (std::size_t idx = 0; idx < size; ++idx) {
//         if (achievables[idx]) {
//             std::cout << idx << " ";
//         }
//     }
//     std::cout << std::endl;

//     return 0;
// };
