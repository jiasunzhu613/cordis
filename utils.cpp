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

void buf_append(std::vector<uint8_t> &vec, const uint8_t *data, size_t len) {
    vec.insert(vec.end(), data, data + len); // adds using pointers to start and end of data
}
void buf_consume(std::vector<uint8_t> &vec, size_t n) {
    vec.erase(vec.begin(), vec.begin() + n); // erase beginning till n of vector using iterator
} // consume n from the start
