#pragma once

#include "adjacency_matrix/abc.h"
#include "adjacency_matrix/csr.h"


template<class TSize = std::size_t, class TCoef = float, class TMatrix = CSRAdjacencyMatrix<TCoef, TSize>, class TEdgeIterator = CSREdgeIterator<TSize, TCoef>>
class RemoveEdges {
protected:
    TMatrix& _matrix;
protected:
    _run(TCoef (TMatrix::*set)(TSize, TSize, TCoef)){
        for(TEdgeIterator iterator = this->_matrix.iterator(); !iterator.stopped(); ++iterator){
            
        }
    }



public:
    RemoveEdges(TMatrix& matrix): _matrix(matrix){};

    void copy(TMatrix& from, TMatrix& to){
        for(TSize ){}
    }

    virtual void run(){}



};