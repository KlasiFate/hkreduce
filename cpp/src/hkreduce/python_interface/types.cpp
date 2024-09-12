#define PY_SSIZE_T_CLEAN

#include <cstring>

#include "Python.h"
#include "structmember.h"

#include "numpy/ndarrayobject.h"
#include "numpy/ndarraytypes.h"

#include "hkreduce/adjacency_matrix/csr.h"
#include "hkreduce/collections/array_based.h"
#include "hkreduce/collections/sectioned.h"
#include "hkreduce/collections/algorithms.h"
#include "hkreduce/reducing/drg.h"
#include "hkreduce/reducing/drgep.h"
#include "hkreduce/reducing/pfa.h"


typedef struct {
    PyObject_HEAD
        size_t nextRowIdx;
    bool finalized;
    CSRAdjacencyMatrix<double>* matrix;
} CSRAdjacencyMatrixObject;

static void CSRAdjacencyMatrixObject_dealloc(CSRAdjacencyMatrixObject* self) {
    if (self->matrix != nullptr && self->matrix != NULL) {
        Allocator* allocator = getDefaultAllocator();
        allocator->deallocate((void*) self->matrix);
    }
}

static PyObject* CSRAdjacencyMatrixObject_new(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    CSRAdjacencyMatrixObject* self;
    self = (CSRAdjacencyMatrixObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->matrix = nullptr;
        self->finalized = false;
    }
    return (PyObject*) self;
}

static int CSRAdjacencyMatrixObject_init(CSRAdjacencyMatrixObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] {
        "size",
        NULL
    };
    // Py_ssize_t is same as ssize_t
    Py_ssize_t sizeNotRebound;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "n:__init__", kwlist, &sizeNotRebound)) {
        return -1;
    }
    size_t size = (size_t) sizeNotRebound;

    Allocator* allocator = getDefaultAllocator();

    IndexableCollection<size_t>* rows = nullptr;
    IndexableCollection<size_t>* cols = nullptr;
    IndexableCollection<double>* coefs = nullptr;
    self->matrix = nullptr;

    try {
        rows = (ArrayCollection<size_t>*) allocator->allocate(sizeof(ArrayCollection<size_t>));
        cols = (SectionedCollection<size_t>*) allocator->allocate(sizeof(SectionedCollection<size_t>));
        coefs = (SectionedCollection<double>*) allocator->allocate(sizeof(SectionedCollection<double>));
        self->matrix = (CSRAdjacencyMatrix<double>*) allocator->allocate(sizeof(CSRAdjacencyMatrix<double>));
    }
    catch (const bad_alloc& error) {
        if (rows != nullptr) {
            allocator->deallocate((void*) rows);
        }
        if (cols != nullptr) {
            allocator->deallocate((void*) cols);
        }
        if (coefs != nullptr) {
            allocator->deallocate((void*) coefs);
        }
        if (self->matrix != nullptr) {
            allocator->deallocate((void*) self->matrix);
        }
        return -1;
    }

    try {
        new (rows) ArrayCollection<size_t>(size, 0, allocator);
        new (cols) SectionedCollection<size_t>(0, allocator);
        new (coefs) SectionedCollection<double>(0, allocator);
        new (self->matrix) CSRAdjacencyMatrix<double>(rows, cols, coefs, true, allocator);
    }
    catch (const exception& error) {
        PyErr_SetString(PyExc_RuntimeError, error.what());
        return -1;
    }    
catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown error");
        return -1;
    }

    return 0;
}

static PyObject* CSRAdjacencyMatrixObject_add_row(CSRAdjacencyMatrixObject* self, PyObject* args) {
    PyArrayObject* array = NULL;
    Py_ssize_t rowIdxNotRebound;

    if (!PyArg_ParseTuple(args, "On:add_row", &array, &rowIdxNotRebound)) {
        return NULL;
    }
    if (!PyObject_IsInstance((PyObject*) array, (PyObject*) &PyArray_Type)) {
        PyErr_SetString(PyExc_TypeError, "An array object of the \"numpy.ndarray\" type is expected");
        return NULL;
    }
    if (PyArray_NDIM(array) != 1) {
        PyErr_SetString(PyExc_ValueError, "Count of the array's dimensions doesn't equal 1");
        return NULL;
    }
    if (PyArray_TYPE(array) != NPY_DOUBLE) {
        PyErr_SetString(PyExc_TypeError, "The array's type doesn't equal double");
        return NULL;
    }
    if (((size_t) PyArray_DIM(array, 0)) != self->matrix->getSize()) {
        PyErr_SetString(PyExc_ValueError, "The array's length doesn't equal matrix size");
        return NULL;
    }
    if (PyArray_FLAGS(array) | NPY_C_CONTIGUOUS == 0) {
        PyErr_SetString(PyExc_ValueError, "The array is not in C format of storing data");
        return NULL;
    }

    size_t rowIdx = (size_t) rowIdxNotRebound;
    if (self->nextRowIdx > rowIdx) {
        PyErr_SetString(PyExc_ValueError, "Rows can be added only in ascending order of its index");
        return NULL;
    }
    if (rowIdx >= self->matrix->getSize()) {
        PyErr_SetString(PyExc_IndexError, "Index of row is greater or equal than matrix size");
        return NULL;
    }
    if (self->nextRowIdx == self->matrix->getSize()) {
        PyErr_SetString(PyExc_ValueError, "All rows was added");
        return NULL;
    }

    const double* data = (double*) PyArray_DATA(array);
    IndexableCollection<size_t>& rows = *self->matrix->getRows();
    IndexableCollection<size_t>& cols = *self->matrix->getCols();
    IndexableCollection<double>& coefs = *self->matrix->getCoefs();

    size_t nonZeroCols = 0;
    for (size_t colIdx = 0; colIdx < self->matrix->getSize(); ++colIdx) {
        const double element = data[colIdx];
        if (element != 0) {
            ++nonZeroCols;
            cols.append(colIdx);
            coefs.append(element);
        }
    }

    rows[rowIdx] = nonZeroCols;

    self->nextRowIdx = rowIdx + 1;

    return Py_None;
}

static PyObject* CSRAdjacencyMatrixObject_finalize(CSRAdjacencyMatrixObject* self, PyObject* args) {
    if(self->finalized){
        PyErr_SetString(PyExc_ValueError, "Already finalized");
    }
    try {
        size_t accumulate = 0;
        IndexableCollection<size_t>& rows = *self->matrix->getRows();
        for (size_t i = 0; i < self->matrix->getSize(); ++i) {
            accumulate += rows[i];
            rows[i] = accumulate;
        }
    }
    catch (const exception& error) {
        PyErr_SetString(PyExc_RuntimeError, error.what());
        return NULL;
    }
    catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown error");
        return NULL;
    }
    return Py_None;
};

static PyObject* CSRAdjacencyMatrixObject_run_reducing(CSRAdjacencyMatrixObject* self, PyObject* args) {
    char method[6]; // len("DRGEP") + 1 == 6
    double threshold;
    PyArrayObject* sourcesNumpyArray = NULL;

    if (!PyArg_ParseTuple(args, "sdO:run_reducing", &method, &threshold, &sourcesNumpyArray)) {
        return NULL;
    }

    if (!PyObject_IsInstance((PyObject*) sourcesNumpyArray, (PyObject*) &PyArray_Type)) {
        PyErr_SetString(PyExc_TypeError, "An array object of the \"numpy.ndarray\" type is expected");
        return NULL;
    }
    if (PyArray_NDIM(sourcesNumpyArray) != 1) {
        PyErr_SetString(PyExc_ValueError, "Count of the array's dimensions doesn't equal 1");
        return NULL;
    }
    if (PyArray_TYPE(sourcesNumpyArray) != NPY_UINTP) {
        PyErr_SetString(PyExc_TypeError, "The array's type doesn't equal double");
        return NULL;
    }

    size_t sourcesNumpyArraySize = (size_t) PyArray_DIM(sourcesNumpyArray, 0);
    if (!self->finalized){
        PyErr_SetString(PyExc_ValueError, "Matrix is not finalized");
    }
    if (sourcesNumpyArraySize > self->matrix->getSize()) {
        PyErr_SetString(PyExc_ValueError, "The array's length is greater than matrix size");
        return NULL;
    }
    if (PyArray_FLAGS(sourcesNumpyArray) | NPY_C_CONTIGUOUS == 0) {
        PyErr_SetString(PyExc_ValueError, "The array is not in C format of storing data");
        return NULL;
    }

    PyArrayObject* resultArray = NULL;
    try {
        ArrayCollection<size_t> sources(sourcesNumpyArraySize, getDefaultAllocator());
        for (size_t i = 0; i < sourcesNumpyArraySize; ++i) {
            size_t* idx = (size_t*) PyArray_GETPTR1(sourcesNumpyArray, (npy_intp) i);
            sources.append(*idx);
        }

        Bitmap resultBitmap;
        if (strcmp(method, "DRG")) {
            DRG<double> drg;
            resultBitmap = drg.run(
                *(self->matrix),
                sources,
                threshold,
                getDefaultAllocator()
            );
        }
        else if (strcmp(method, "DRGEP")) {
            DRGEP<double> drgep;
            resultBitmap = drgep.run(
                *(self->matrix),
                sources,
                threshold,
                getDefaultAllocator()
            );
        }else {
            PFA<double> pfa;
            resultBitmap = pfa.run(
                *(self->matrix),
                sources,
                threshold,
                getDefaultAllocator()
            );
        }

        size_t resultArraySize = countBits(resultBitmap, true);
        const npy_intp dims[1] { (npy_intp) resultArraySize };
        resultArray = (PyArrayObject*) PyArray_NewFromDescr(
            &PyArray_Type,
            PyArray_DescrFromType(NPY_UINTP),
            1,
            dims,
            NULL,
            NULL,
            NPY_ARRAY_C_CONTIGUOUS, // 0
            NULL
        );

        for (size_t i = 0; i < self->matrix->getSize(); ++i) {
            if (resultBitmap[i]) {
                size_t* idx = (size_t*) PyArray_GETPTR1(resultArray, (npy_intp) i);
                *idx = i;
            }
        }
    }catch (const exception& error) {
        PyErr_SetString(PyExc_RuntimeError, error.what());
        Py_XDECREF(resultArray);
        return NULL;
    }
    catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown error");
        Py_XDECREF(resultArray);
        return NULL;
    }
    return (PyObject*) resultArray;
};

static PyMemberDef CSRAdjacencyMatrixObject_members[] = {
    {NULL}
};

static PyMethodDef CSRAdjacencyMatrixObject_methods[] = {
    {
        "add_row",
        (PyCFunction) CSRAdjacencyMatrixObject_add_row,
        METH_VARARGS,
        "Add row to CSR a matrix"
    },
    {
        "finalize",
        (PyCFunction) CSRAdjacencyMatrixObject_finalize,
        METH_NOARGS,
        "Finalize a matrix object. It accumulate rows array elements"
    },
    {
        "run_reducing",
        (PyCFunction) CSRAdjacencyMatrixObject_run_reducing,
        METH_VARARGS,
        "Run reducing for a matrix"
    },
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyTypeObject CSRAdjacencyMatrixType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "hkreduce.cpp_interface.CSRAdjacencyMatrix",
    .tp_basicsize = sizeof(CSRAdjacencyMatrixObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor) CSRAdjacencyMatrixObject_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = PyDoc_STR("CSR adjacency matrix, that store data"),
    .tp_methods = CSRAdjacencyMatrixObject_methods,
    .tp_members = CSRAdjacencyMatrixObject_members,
    .tp_init = (initproc) CSRAdjacencyMatrixObject_init,
    .tp_new = CSRAdjacencyMatrixObject_new,
};

static PyMethodDef cpp_interface_methods[] = {
    {NULL, NULL, 0, NULL}
};

static PyModuleDef c_interface_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "hkreduce.cpp_interface",
    .m_doc = "Module that provide python interface to interact with c++ layer",
    .m_size = -1,
    .m_methods = cpp_interface_methods
};

PyMODINIT_FUNC
PyInit_cpp_interface(void) {
    PyObject* m;
    if (PyType_Ready(&CSRAdjacencyMatrixType) < 0) {
        return NULL;
    }

    m = PyModule_Create(&c_interface_module);
    if (m == NULL) {
        return NULL;
    }

    if (PyModule_AddObjectRef(m, "CSRAdjacencyMatrix", (PyObject*) &CSRAdjacencyMatrixType) < 0) {
        Py_DECREF(m);
        return NULL;
    }

    return m;
}