#include "utils.hpp"

int read_all(int fd, char *buf, size_t n) {
    // loop which there are still bytes to read
    while (n > 0) {
        // try to read
        int rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1; // error
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

int write_all(int fd, char *buf, size_t n) {
    // loop which there are still bytes to read
    while (n > 0) {
        // try to read
        int rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1; // error
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

void buf_append(struct Buffer *buf, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // not enough space, need reallocation
        if (buf->data_end > buf->buffer_end) {
            buf->realloc();
        }

        // iteratively append values onto underlying uint8_t array buffer
        *(buf->data_end++) = data[i];
    }
}

void buf_consume(struct Buffer *buf, size_t n) {
    // Shift data pointer backwards
    // First check if n is too big
    size_t data_len = buf->data_end - buf->data_begin;
    if (n > data_len) {
        n = data_len;
    }

    buf->data_begin += n;
} // consume n from the start

// to emit int into buffer stream
void buf_append_i64(struct Buffer *buf, int64_t val) {
    buf_append(buf, (uint8_t*)&val, 8); // emit 8 bytes
}

// to emit length into buffer stream
void buf_append_u32(struct Buffer *buf, uint32_t val) {
    buf_append(buf, (uint8_t*)&val, 4); // emit 4 bytes
}

// to emit tag into buffer stream
void buf_append_u8(struct Buffer *buf, uint8_t val) {
    buf_append(buf, &val, 1); // emit 1 byte
}

void emit_crlf(struct Buffer *buf) {
    buf_append_u8(buf, '\r');
    buf_append_u8(buf, '\n');
}

void emit_null(struct Buffer *buf) {
    buf_append_u8(buf, (uint8_t)TAG_NULL);
    emit_crlf(buf);
}

void emit_simple_string(struct Buffer *buf, uint8_t *s, size_t len) {
    buf_append_u8(buf, (uint8_t)TAG_SIMPLE_STRING);
    buf_append(buf, s, len);
    emit_crlf(buf);
}

void emit_simple_error(struct Buffer *buf, uint8_t *err, size_t len) {
    buf_append_u8(buf, (uint8_t)TAG_SIMPLE_ERROR);
    buf_append(buf, err, len);
    emit_crlf(buf);
}

void emit_int64(struct Buffer *buf, int64_t num) {  
    // TODO: update this to ascii later
    buf_append_u8(buf, (uint8_t)TAG_INT64);
    buf_append_i64(buf, num);
    emit_crlf(buf);
}

void emit_bulk_string(struct Buffer *buf, uint8_t *s, size_t len) {
    // serialization format: Tag, length, crlf, data, crlf
    buf_append_u8(buf, (uint8_t)TAG_BULK_STRING);
    buf_append_u32(buf, (uint32_t)len);
    emit_crlf(buf);
    buf_append(buf, s, len);
    emit_crlf(buf);
}

void emit_array(struct Buffer *buf, size_t len) {
    buf_append_u8(buf, (uint8_t)TAG_ARRAY);
    buf_append_u32(buf, (uint32_t)len);
    emit_crlf(buf);
}


int read_tag(struct Buffer *buf, Tags &tag) {
    uint8_t tag_value;
    memcpy(&tag_value, buf->data(), 1); // copy value in and check for validity of tag
    // consume buffer
    printf("read_tag => remaining buffer size: %lu\n", buf->size());
    // URGENT: REALLY BAD FOR PIPELINING, COULD BREAK THIGNS...
    // TODO: shouldnt consume in case not enough info
    buf_consume(buf, 1); // consume 1 byte

    if (tag_value != '_' && tag_value != '-' && tag_value != '+' &&
            tag_value != ':' && tag_value != '$' && tag_value != '*') {
        printf("read_tag => returned bad val of: %u\n", tag_value);
        return -1;
    }

    tag = static_cast<Tags>(tag_value);
    return 0;
}

// mainly onyl used for types that have a length within the serialization format
int read_crlf(struct Buffer *buf) {
    std::string crlf;
    crlf.assign(buf->data(), buf->data() + 2);
    // consume buffer
    buf_consume(buf, 2);

    if (crlf != "\r\n") {
        return -1;
    }
    
    return 0;
}

void read_null(struct Buffer *buf); // TODO
// TODO: should this be uint8_t?
// These two we read by iteratively parsing
// read into a pointer
int read_simple_string(struct Buffer *buf, std::string &ptr) {
    // start reading, when we hit \r, we check immediately for \n. If we got CRLF, we stop, else we error
    // Read in non-copying manner by stepping through the buffer
    uint8_t *curr = buf->data();
    while (curr < buf->data_end) {
        if (*curr == '\r') {
            if (curr + 1 > buf->data_end || *(curr + 1) != '\n') {
                return -1;
            }
            break;
        }
        curr++;
    }

    ptr.assign(buf->data(), curr); // assign payload to ptr
    buf_consume(buf, curr - buf->data());
    // crlf read technically should not fail
    if (read_crlf(buf) < 0) {
        return -1;
    }
    return 0;
}

int read_simple_error(struct Buffer *buf, std::string &ptr) {
    return read_simple_string(buf, ptr);
}

void read_int64(struct Buffer *buf, int64_t &ptr); // TODO
int read_bulk_string(struct Buffer *buf, std::string &ptr) {
    uint32_t len;
    if (buf->size() < 4) {
        return -1;
    }

    memcpy(&len, buf->data(), 4);
    buf_consume(buf, 4);
    if (read_crlf(buf) < 0) {
        return -1;
    }

    if (buf->size() < len) {
        return -1;
    }

    ptr.assign(buf->data(), buf->data() + len);
    buf_consume(buf, len);
    if (read_crlf(buf) < 0) {
        return -1;
    }

    return 0;
}

// CURRENT: just read array length into ptr
int read_array_length(struct Buffer *buf, uint32_t &ptr) {
    if (buf->size() < 4) {
        return -1;
    }

    memcpy(&ptr, buf->data(), 4);
    buf_consume(buf, 4);
    if (read_crlf(buf) < 0) {
        return -1;
    }

    return 0;
}

// read back one Cordis_Response object
int read_one(struct Buffer *buf, struct Cordis_Response *resp) {
    if (read_tag(buf, resp->tag) < 0) {
        return -1;
    }

    // Simple recursive descent parser
    switch (resp->tag) {
    case Tags::TAG_SIMPLE_STRING:
        if (read_simple_string(buf, resp->simple_string) < 0) {
            return -1;
        }
        break;
    case Tags::TAG_SIMPLE_ERROR:
        if (read_simple_error(buf, resp->simple_error) < 0) {
            return -1;
        }
        break;
    case Tags::TAG_BULK_STRING:
        if (read_bulk_string(buf, resp->bulk_string) < 0) {
            return -1;
        }
        break;
    case Tags::TAG_ARRAY:
        uint32_t len;
        if (read_array_length(buf, len) < 0) {
            return -1;
        }

        for (uint32_t i = 0; i < len; i++) {
            resp->array.push_back(Cordis_Response{});

            if (read_one(buf, &resp->array.back()) < 0) {
                return -1;
            }
        }
        break;
    default:
        break;
    }
    
    return 0;
}