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
