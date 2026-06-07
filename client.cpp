#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>
#include <errno.h>

#include "utils.hpp"

void vec_buf_append(std::vector<uint8_t> &vec, const uint8_t *data, size_t len) {
    vec.insert(vec.end(), data, data + len); // adds using pointers to start and end of data
}
void vec_buf_consume(std::vector<uint8_t> &vec, size_t n) {
    vec.erase(vec.begin(), vec.begin() + n); // erase beginning till n of vector using iterator
} // consume n from the start

static int send_req(int fd, std::vector<std::string> data, size_t len) {
    size_t total_payload_size = (len + 1) * 4;
    for (std::string &s : data) {
        total_payload_size += s.length();
    }

    if (len > MAX_MSG_SIZE) {
        return -1;
    }
    
    std::vector<uint8_t> buf;
    vec_buf_append(buf, (const uint8_t *)&total_payload_size, 4); // payload len
    vec_buf_append(buf, (const uint8_t *)&len, 4);

    for (std::string &s : data) {
        size_t str_len = s.length();
        vec_buf_append(buf, (const uint8_t *)&str_len, 4);
        vec_buf_append(buf, (const uint8_t *)s.c_str(), str_len);
    }

    return write_all(fd, (char *)buf.data(), buf.size());
}

static int read_res(int fd) {
    std::vector<uint8_t> rbuf;
    rbuf.resize(4); // resize to 4 byte sized to read one integer

    // read length header first
    errno = 0;
    int err = read_all(fd, (char*)&rbuf[0], 4);
    if (err) {
        if (errno == 0) {
            perror("EOF");
        } else {
            perror("read() failed at reading header");
        }

        return err;
    }

    // copy bytes into len
    uint32_t len;
    memcpy(&len, rbuf.data(), 4); // REMEMBER: this is a c api so we need to use .data() to get underlying array from vector
    
    // check if len header is too long!
    if (len > MAX_MSG_SIZE) {
        return -1;
    }

    // start reading status
    rbuf.resize(4 + 4);
    err = read_all(fd, (char*)&rbuf[4], 4);
    if (err) {
        perror("read() error at reading status");
        return err;
    }

    uint32_t status;
    memcpy(&status, &rbuf[4], 4);

    // start reating actual response data
    rbuf.resize(4 + 4 + len);
    err = read_all(fd, (char*)&rbuf[8], len - 4);
    if (err) {
        perror("read() error at reading data");
        return err;
    }

    // We limit the amount of chars we print
    printf("Server sent: status=%u, data=%.*s\n", status, len < 100 ? len : 100, &rbuf[8]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket() failed");
        return 1;
    }

    // Connect to host + port
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5678);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // bind to loopback address 127.0.0.1
    int rv = connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    if (rv) {
        perror("connect() failed");
        return 1;
    }

    // char msg[] = "redis says hello";
    // write(fd, msg, sizeof(msg)); // write message to socket

    // char rbuf[64] = {};
    // int n_recv = read(fd, rbuf, sizeof(rbuf) - 1);
    // if (n_recv < 0) {
    //     perror("read() failed");
    //     return 1;
    // }

    // printf("Server wrote: %s\n", rbuf)
    std::vector<std::string> payloads = {
        "set", "a", "hello world"
        // std::string(MAX_MSG_SIZE, 'z'), 
        // "hello3"
    };

    std::vector<std::string> payloads2 = {
        "get", "a"
        // std::string(MAX_MSG_SIZE, 'z'), 
        // "hello3"
    };

    // Likely will pipeline the requests as the server will likely read some number of data together
    // Send all requests to pipeline
    int err = send_req(fd, payloads, payloads.size());
    if (err) {
        close(fd);
        return 1;
    }
    int err2 = read_res(fd);
    if (err2) {
        close(fd);
        return 1;
    }

    err = send_req(fd, payloads2, payloads2.size());
    if (err) {
        close(fd);
        return 1;
    }
    err2 = read_res(fd);
    if (err2) {
        close(fd);
        return 1;
    }

    return 0;
}