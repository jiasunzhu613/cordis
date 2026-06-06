#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>
#include <errno.h>

#include "utils.hpp"

// static int send_one_request(int fd, const char *text, size_t text_len) {
//     printf("hello entered");
//     if (text_len > MAX_MSG_SIZE) {
//         return -1;
//     }

//     char wbuf[4 + MAX_MSG_SIZE] = {};
//     memcpy(wbuf, &text_len, 4);
//     memcpy(&wbuf[4], text, text_len);

//     // write to socket
//     write_all(fd, wbuf, 4 + text_len);

//     // Read from server
//     char rbuf[4 + MAX_MSG_SIZE] = {};
//     int err = read_all(fd, rbuf, 4);
//     if (err) {
//         perror("read() error");
//         return err;
//     }

//     int msg_len;
//     memcpy(&msg_len, rbuf, 4);

//     err = read_all(fd, &rbuf[4], msg_len);
//     if (err) {
//         perror("read() content error");
//         return err;
//     }

//     // printf("Server wrote: %s\n", &rbuf[4]);
//     return 0;
// }

static int send_req(int fd, const uint8_t *data, size_t len) {
    if (len > MAX_MSG_SIZE) {
        return -1;
    }

    std::vector<uint8_t> buf;
    // WTF is this doing?
    buf_append(buf, (const uint8_t *)&len, 4);
    buf_append(buf, data, len);

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

    rbuf.resize(4 + len);
    // start reading actual body returned
    err = read_all(fd, (char*)&rbuf[4], len);
    if (err) {
        perror("read() error at reading payload");
        return err;
    }

    // We limit the amount of chars we print
    printf("Server sent: length=%u, data=%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);
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
        "a", "b", 
        std::string(MAX_MSG_SIZE, 'z'), 
        "hello3"
    };

    // Likely will pipeline the requests as the server will likely read some number of data together
    // Send all requests to pipeline
    for (const std::string& s : payloads) {
        int err = send_req(fd, (uint8_t*)s.data(), s.size());
        if (err) {
            close(fd);
            return 1;
        }
    }

    // NOTE: .size() returns size_t!!!
    // later read!
    for (size_t i = 0; i < payloads.size(); i++) {
        int err = read_res(fd);
        if (err) {
            close(fd);
            return 1;
        }
    }

    return 0;
}