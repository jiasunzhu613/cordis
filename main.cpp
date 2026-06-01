#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <cerrno>

#include "utils.hpp"

// static void do_stuff(int client_fd) {
//     char rbuf[64] = {};
//     ssize_t n_recv = read(client_fd, rbuf, sizeof(rbuf) - 1); // -1 for null-terminator
//     if (n_recv < 0) {
//         perror("read() error");
//         return;
//     }

//     printf("client wrote: %s\n", rbuf);

//     char wbuf[] = "world";
//     write(client_fd, wbuf, sizeof(wbuf));
// }

// Process protocol header first then send back adhering to protocol
static int process_one_request(int client_fd) {
    char rbuf[4 + MAX_MSG_SIZE] = {};
    // Process 4 byte sized length header
    errno = 0; // we make sure to set errno to 0 since errno does not update on success
    int err = read_all(client_fd, rbuf, 4);
    if (err) {
        perror(errno == 0 ? "EOF" : "ERROR");
        return -1;
    }

    int len;
    memcpy(&len, rbuf, 4); // copy bytes into len

    // Read actual content
    err = read_all(client_fd, &rbuf[4], len);
    if (err) {
        perror("read() failed");
        return -1;
    }
    printf("Client wrote: %s\n", &rbuf[4]);

    // Send back message following same protocols
    const char resp[] = "world";
    char wbuf[4 + MAX_MSG_SIZE] = {};
    int resp_len = strlen(resp);

    // memcpy in reply length and reply
    memcpy(wbuf, &resp_len, 4);
    memcpy(&wbuf[4], resp, resp_len);

    return write_all(client_fd, wbuf, 4 + resp_len); // why not sizeof(wbuf)
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Created socket: %d\n", fd);

    int val = 1;
    // Sets SO_REUSEADDR to true in order to let server reuse socket fd on close?
    // Related: https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux/3233022#3233022
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // Bind socket to port and host domain
    // we bind to 0.0.0.0:5678
    // Structure of socket structs
    // struct sockaddr_in {
    //     uint16_t       sin_family; // AF_INET
    //     uint16_t       sin_port;   // port in big-endian
    //     struct in_addr sin_addr;   // IPv4
    // };
    // struct in_addr {
    //     uint32_t       s_addr;     // IPv4 in big-endian
    // };
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5678);
    addr.sin_addr.s_addr = htonl(0);
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        perror("failed to bind host + port");
        return 1;
    }

    printf("Bound: %d\n", rv);

    // Enable socket to listen
    rv = listen(fd, SOMAXCONN); // SOMAXCONN => 4096; defines the size of the listen queue

    // Let socket accept connections
    while (1) {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t addr_len = sizeof(client_addr); // defines both input and output socket length, need to match IP version
        int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len); // accept() is treated as a read() call

        if (client_fd < 0) {
            continue; // error
        }

        while (1) {
            // Note: we use errno to determine if we exited gracefully 
            // or if we exited because something actually when wrong
            int err = process_one_request(client_fd);
            if (err) {
                break;
            }
        }
        close(client_fd);
    }
}