#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "utils.hpp"

static int send_one_request(int fd, const char *text) {
    int text_len = strlen(text);
    if (text_len > MAX_MSG_SIZE) {
        return -1;
    }

    char wbuf[4 + MAX_MSG_SIZE] = {};
    memcpy(wbuf, &text_len, 4);
    memcpy(&wbuf[4], text, text_len);

    // write to socket
    write_all(fd, wbuf, 4 + text_len);

    // Read from server
    char rbuf[4 + MAX_MSG_SIZE] = {};
    int err = read_all(fd, rbuf, 4);
    if (err) {
        perror("read() error");
        return err;
    }

    int msg_len;
    memcpy(&msg_len, rbuf, 4);

    err = read_all(fd, &rbuf[4], msg_len);
    if (err) {
        perror("read() content error");
        return err;
    }

    printf("Server wrote: %s\n", &rbuf[4]);
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

    // printf("Server wrote: %s\n", rbuf);
    const char msg1[] = "redis says hello";
    int err = send_one_request(fd, msg1);
    if (err) {
        close(fd);
        return 0;
    }

    const char msg2[] = "redis says yoo";
    err = send_one_request(fd, msg2);
    if (err) {
        close(fd);
        return 0;
    }
}