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