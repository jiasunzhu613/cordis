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


const int MAX_MSG_SIZE = 32 << 20; // 33554432

// Unscoped enum for defining tag types for RESP2 protocol
// NOTE: all enums are 1 byte
enum Tags {
    TAG_NULL = '_', // RESP3 standard NULL tag type
    TAG_SIMPLE_STRING = '+',
    TAG_SIMPLE_ERROR = '-',
    TAG_INT64 = ':',
    TAG_BULK_STRING = '$', // TODO: just make this handle RESP2 historic null bulk string too
    TAG_ARRAY = '*',
};

struct Cordis_Response {
    Tags tag;

    // Placeholder values for each tag type
    std::string simple_string;
    std::string simple_error;
    uint64_t integer;
    std::string bulk_string;
    std::vector<struct Cordis_Response> array;
};

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

// TODO: add helper functions to emit different serialized types into buffer
// NOTE: currently, we emit integers as binary bytes into the buffer stream.
//       The actual RESP2 serialization protocol emits integers as ASCII encoded bytes
void buf_append_i64(struct Buffer *buf, int64_t val); // to emit int into buffer stream
void buf_append_u32(struct Buffer *buf, uint32_t val); // to emit length into buffer stream
void buf_append_u8(struct Buffer *buf, uint8_t val); // to emit tag into buffer stream

void emit_crlf(struct Buffer *buf);
void emit_null(struct Buffer *buf);
// TODO: can probably consolidate these two together
void emit_simple_string(struct Buffer *buf, uint8_t *s, size_t len);
void emit_simple_error(struct Buffer *buf, uint8_t *err, size_t len);
void emit_int64(struct Buffer *buf, int64_t num);
void emit_bulk_string(struct Buffer *buf, uint8_t *s, size_t len);
void emit_array(struct Buffer *buf, size_t len); 

// Helpers for deserializing
// Process: read tag, then offload to separate read function
int read_tag(struct Buffer *buf, Tags &tag); 
int read_crlf(struct Buffer *buf); // mainly onyl used for types that have a length within the serialization format

void read_null(struct Buffer *buf);
// TODO: should this be uint8_t?
// These two we read by iteratively parsing
int read_simple_string(struct Buffer *buf, std::string &ptr); // read into a pointer
int read_simple_error(struct Buffer *buf, std::string &ptr );
void read_int64(struct Buffer *buf, int64_t &ptr);
int read_bulk_string(struct Buffer *buf, std::string &ptr);
int read_array_length(struct Buffer *buf, uint32_t &ptr); // CURRENT: just read array length into ptr
int read_one(struct Buffer *buf, struct Cordis_Response *resp); // read back one Cordis_Response object