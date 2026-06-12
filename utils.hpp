#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <string>
#include <iostream>


const int MAX_MSG_SIZE = 32 << 20;

// TODO: support void *
int read_all(int fd, char *buf, size_t n);
int write_all(int fd, char *buf, size_t n);

// Buffer structure
// Maybe just have a vector and then keep track of start for optimizing consume operation
struct Buffer {
    // Buffer covers the entirety allocated capacity
    // TODO: when first assigning buffer, malloc to buffer_begin
    // if we use vector for buffer_begin => resizing is handled by std::vector
    //  => mem address will change just like in c-style reallocs
    // how to change data_begin? => 
    size_t curr_size;
    uint8_t *buffer_begin;
    uint8_t *buffer_end;

    // Data covers the actual used components of the capacity
    uint8_t *data_begin;
    uint8_t *data_end;

    Buffer(size_t size) {
        curr_size = size;
        buffer_begin = new uint8_t[size];
        buffer_end = buffer_begin + size;
        data_begin = buffer_begin;
        data_end = buffer_begin;
    }

    // Use delegating constructor
    Buffer() : Buffer(1024) {}

    ~Buffer() {
        delete[] buffer_begin;
    }

    // Call with instance.realloc()
    void realloc() {
        size_t new_size = curr_size * 2; // realloc 
        // Create new buffer of increased size
        uint8_t *new_buffer = new uint8_t[new_size];
        // size_t data_offset = data_begin - buffer_begin; // get how far up actual data is from the start of the buffer
        size_t data_size = data_end - data_begin; // how much data we have right now

        // just copy data over
        // essentially the same as triggering a realloc and a forward shift in data
        memcpy(new_buffer, data_begin, data_size);

        data_begin = new_buffer;
        data_end = new_buffer + data_size;

        delete[] buffer_begin; // free existing uint8_t buffer
        buffer_begin = new_buffer;
        buffer_end = buffer_begin + new_size;
        curr_size = new_size;
    }

    // Mimic std::vector STL functions
    size_t size() {
        return data_end - data_begin;
    }

    uint8_t *data() {
        return data_begin;
    }
};

// NOTE: vectors are passed by value (copied entirely) by default in cpp
void buf_append(struct Buffer *buf, const uint8_t *data, size_t len);
void buf_consume(struct Buffer *buf, size_t n); // consume n from the start
