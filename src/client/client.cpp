#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>
#include <errno.h>

#include <common/utils.hpp>

// send request in array bulk string format
static int send_req(int fd, std::vector<std::string> data, size_t len) {
    Buffer buf;

    emit_array(&buf, len);
    for (std::string &s : data) {
        emit_bulk_string(&buf, (uint8_t*)s.data(), s.length());
    }

    printf("\nSent request\n");
    return write_all(fd, (char *)buf.data(), buf.size());
}

// TODO: read all into rbuf then move into Buffer structure
static int read_data(int fd, Buffer *buf) {
    uint8_t rbuf[64 * 1024];

    // read length header first
    errno = 0;
    int rv = read(fd, rbuf, sizeof(rbuf));
    if (rv <= 0) {
        if (errno == 0) {
            perror("EOF");
        } else {
            perror("read() failed at data");
        }

        return -1;
    }

    buf_append(buf, rbuf, (size_t)rv);
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
    Buffer buf;
    int err = send_req(fd, payloads, payloads.size());
    if (err) {
        close(fd);
        return 1;
    }
    int err1 = read_data(fd, &buf);
    if (err1) {
        close(fd);
        return 1;
    }
    Cordis_Response response1;
    int cordis_err = read_one(&buf, &response1);
    if (cordis_err < 0) {
        close(fd);
        return 1;
    }
    printf("Tag type: %c, response: %s\n", response1.tag, response1.simple_string.c_str());

    err = send_req(fd, payloads2, payloads2.size());
    if (err) {
        close(fd);
        return 1;
    }
    err1 = read_data(fd, &buf);
    if (err1) {
        close(fd);
        return 1;
    }
    Cordis_Response response2;
    cordis_err = read_one(&buf, &response2);
    if (cordis_err < 0) {
        close(fd);
        return 1;
    }
    printf("Tag type: %c, response: %s\n", response2.tag, response2.bulk_string.c_str());

    return 0;
}