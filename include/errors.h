#pragma once
#include <stdexcept>
#include <cstdio>

#define MAX_LENGTH_MSG 1024

template<class... T>
class ValueError : public std::runtime_error {
    using std::runtime_error::runtime_error;
protected:
    const char* _buffer = nullptr;
public:
    ValueError<>(const char* msg, T... args) {
        this->_buffer = new char[MAX_LENGTH_MSG];
        snprintf(this->_buffer, MAX_LENGTH_MSG, msg, args...);
        ValueError<>(this->_buffer);
    }

    ~ValueError<>() {
        delete this->_buffer;
    }
};

template<>
class ValueError<> : public std::runtime_error {
    using std::runtime_error::runtime_error;
};


template<class... T>
class IndexOutOfRangeError : public ValueError<T...> {
    using ValueError<T...>::ValueError;
};


template<class... T>
class PathNotFoundError : public ValueError<T...> {
    using ValueError<T...>::ValueError;
};
