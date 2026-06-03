#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <vector>
#include <fcntl.h>

#include "utils.hpp"

struct Conn {
    int fd;

    // Application level state
    bool want_write;
    bool want_read;
    bool want_close;

    // Data buffers
    std::vector<uint8_t> data_in;
    std::vector<uint8_t> data_out;
};

// What to do?
// Read protocol header first (4 bytes) => can we read?
// try to read body => can we read?
// write result to data_out
static bool try_one_request(Conn *conn) {
    // check data_in size first
    if (conn->data_in.size() < 4) {
        return false; // immediately fail trying to process one request, need to read more
    }

    uint32_t len;
    memcpy(&len, conn->data_in.data(), 4); // directly copy 4 bytes into len

    // ===== IMPORTANT!!! ======
    // Check length of payload sent to ensure that it adheres to the protocol
    if (len > MAX_MSG_SIZE) { // protocol level error
        conn->want_close = true; // close connection
        return false;
    }

    // Try to read full payload
    // Perform length check first 
    if (4 + len > conn->data_in.size()) {
        return false;
    }

    // try to read out all the data
    const uint8_t *request_payload = &conn->data_in[4];

    {
    char msg[len + 1]; // add just for printing purposes, not efficient lul
    memcpy(msg, request_payload, len);
    msg[len] = '\0';
    printf("Client sent: %s\n", msg);
    }

    // (const uint8_t *)&len Reinterprets pointer to len as uint8_t so that when pointer arithmetic is done, we are incrementing by 1 byte instead of 4!
    buf_append(conn->data_out, (const uint8_t *)&len, 4);
    buf_append(conn->data_out, request_payload, len);

    // consume existing buffer data_in
    buf_consume(conn->data_in, 4 + len);

    return true; // success
}

static void fd_set_nb(int fd) {
    // fcntl(fd, F_GETFL, 0) gets the current config of the fd
    // | O_NONBLOCK to set the NONBLOCK bit
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

// Will only be called when socket is ready to be received
static Conn* handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addr_len = sizeof(client_addr); // defines both input and output socket length, need to match IP version
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len); // accept() is treated as a read() call

    if (client_fd < 0) {
        return NULL;
    }

    fd_set_nb(client_fd); // set new fd to nonblocking mode

    Conn *conn = new Conn(); // idiomatic C++ pointer instantiatio
    conn->fd = client_fd;
    conn->want_read = true;

    return conn;
}

static void handle_read(Conn *conn) {
    // NOTE: uint8_t is pretty much char
    uint8_t buf[64 * 1024];

    ssize_t rv = read(conn->fd, buf, sizeof(buf)); // rv is number of bytes received

    // we only handle_read when fd is ready to read so if we read nothing or errored, we just close socket connection
    if (rv <= 0) {
        conn->want_close = true;
        return;
    }

    // Append buffer to data_in buffer of connection
    buf_append(conn->data_in, buf, (size_t) rv);
    
    // try to do a request
    try_one_request(conn); // this will ever only append to data_out on successful protocol parsing

    // switch to write iff we have data to write, because we will only ever be in read or write mode
    if (conn->data_out.size() > 0) {
        conn->want_read = false;
        conn->want_write = true;
    }
}

static void handle_write(Conn *conn) {
    // make sure data_out is non 0
    assert(conn->data_out.size() > 0);
    // write syscall
    ssize_t rv = write(conn->fd, conn->data_out.data(), conn->data_out.size());

    if (rv < 0) { // write errored out
        conn->want_close = true;
        return;
    }

    buf_consume(conn->data_out, (size_t)rv); // consume rv sized buffer from the front

    // switch to read if we have written all data
    if (conn->data_out.size() == 0) {
        conn->want_read = true;
        conn->want_write = false;
    }
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
    std::vector<Conn *> fd_to_conn{}; // Map from fd to Conn struct (note unix fd's are assigned using smallest possible integers)
    std::vector<struct pollfd> poll_args{}; // declares intent for each poll iteration (propagated each time)
    // Let socket accept connections
    while (1) {
        // **prep for poll()**
        // clear poll_args for new iteration
        poll_args.clear();

        // put listening socket as first fd, with POLLIN option
        struct pollfd pfd = {fd, POLLIN, -1};
        poll_args.push_back(pfd);
    
        for (Conn *conn : fd_to_conn) {
            // make sure to be defensive!
            if (!conn) {
                continue;
            }

            // Instantiate pollfd instance for conn
            struct pollfd conn_pfd = {conn->fd, POLLERR, -1};
            // add flags based on conn structure
            if (conn->want_read) {
                conn_pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                conn_pfd.events |= POLLOUT;
            }
            poll_args.push_back(conn_pfd);
        }

        // Call poll()
        // int poll(struct pollfd *fds, nfds_t nfds, int timeout);
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1); 
        if (rv < 0 && errno == EINTR) { // error interupted is not an error, could be caused by signals
            continue;
        }
        if (rv < 0) {
            perror("poll() failed");
            return 1;
        }

        // Handle ready sockets 

        // Handle server listening socket at index 0!
        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                if (fd_to_conn.size() <= (size_t)conn->fd) {
                    fd_to_conn.resize(conn->fd + 1);
                }

                fd_to_conn[conn->fd] = conn;
            }
        }

        // Application level socket callbacks
        for (size_t i = 1; i < poll_args.size(); ++i) {
            uint32_t ready = poll_args[i].revents;
            Conn *conn = fd_to_conn[poll_args[i].fd]; // get fd from poll_args

            // NOTE: we do this because we already propagated the desired want_x state of conn onto poll_args
            if (ready & POLLIN) {
                handle_read(conn);
            }

            if (ready & POLLOUT) {
                handle_write(conn);
            }

            // Always poll for closing the socket connection
            if ((ready & POLLERR) || conn->want_close) {
                close(conn->fd);
                fd_to_conn[conn->fd] = NULL; // set index to NULL
                delete conn; // free pointer
            }
        }
    }
}