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
#include <map>

#include "utils.hpp"
#include "hashtable.hpp"

#define RESP_SUCCESS 0
#define RESP_ERR 1
#define RESP_NOT_FOUND 2

// TODO: maybe add macros for lengths too
#define SIMPLE_STRING_OK "OK"
#define SIMPLE_ERROR_BAD "BAD"
#define SIMPLE_ERROR_CMD_NOT_FOUND "CMD_NOT_FOUND"

// offsetof is a C macro that finds the offset of a member in accordance to a struct in bytes
//    NOTE: the linux version of the container_of macro uses gcc macro extensions to do a member check 
// NOTE: we cast to char* because we want to make byte sized moves to the pointer mem address
// After we cast back to the struct type to allow for proper dereferencing/loading of bytes
#define container_of(ptr, T, member) \
    ((T *)( (char *)ptr - offsetof(T, member) ))

struct Conn {
    int fd;

    // Application level state
    bool want_write;
    bool want_read;
    bool want_close;

    // Data buffers
    Buffer *data_in;
    Buffer *data_out;
};

// TODO: remove
struct Response {
    uint32_t status = 0;
    std::vector<uint8_t> data;
};

struct Entry {
    HashNode node;
    std::string key;
    std::string value;
};

static std::map<std::string, std::string> global_map;
// Global hash table
static HashMap global_hmap = HashMap{};

// FNV hash
// hash = (hash XOR byte) * prime, hash starts as a prime (?)
static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h ^ data[i]) * 0x01000193;
    }
    return h;
}

static bool entry_eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

// custom get, set, del hashtable operations
// Get based on key
static void hmap_do_get(HashMap *hmap, std::vector<std::string> &cmd, Buffer *buf) {
    Entry entry;
    entry.key.swap(cmd[1]);
    entry.node.hash = str_hash((uint8_t*)entry.key.data(), entry.key.length());

    // Now use hash and the hmap to do a lookup for the 
    HashNode *node = hm_lookup(hmap, &entry.node, &entry_eq);
    if (!node) {
        emit_simple_error(buf, (uint8_t*)SIMPLE_ERROR_BAD, 3);
        return;
    }

    Entry *act_entry = container_of(node, struct Entry, node);
    std::string val = act_entry->value;

    // make sure message is not too long
    assert(val.length() <= MAX_MSG_SIZE);
    emit_bulk_string(buf, (uint8_t*)val.data(), val.length());
}

// Set key-value pair
// Here we create new instance
// Response, respond with OK if success, else with error type
static void hmap_do_set(HashMap *hmap, std::vector<std::string> &cmd, Buffer *buf) {
    Entry entry;
    entry.key.swap(cmd[1]);
    entry.node.hash = str_hash((uint8_t*)entry.key.data(), entry.key.length());

    // Now use hash and the hmap to do a lookup for the 
    HashNode *lookup_node = hm_lookup(hmap, &entry.node, &entry_eq);

    // If key is not in hashtable, add it to the hashtable
    if (!lookup_node) {
        Entry *new_entry = new Entry();
        new_entry->key.swap(entry.key);
        new_entry->value.swap(cmd[2]);
        new_entry->node.hash = entry.node.hash;

        // insert the new node into the hash table
        hm_insert(hmap, &new_entry->node);
    } else { // otherwise, it is, so simply change the value
        // Find the container that the hashnode belongs to
        Entry *entry_container = container_of(lookup_node, struct Entry, node);
        entry_container->value.swap(cmd[2]); // change value to new value
    }

    emit_simple_string(buf, (uint8_t*)SIMPLE_STRING_OK, 2);
}

// Del key from hashmap
static void hmap_do_del(HashMap *hmap, std::vector<std::string> &cmd, Buffer *buf) {
    Entry entry;
    entry.key.swap(cmd[1]);
    entry.node.hash = str_hash((uint8_t*)entry.key.data(), entry.key.length());

    // Now use hash and the hmap to do a lookup for the 
    HashNode *node_to_be_deleted = hm_delete(hmap, &entry.node, &entry_eq);

    if (node_to_be_deleted) {
        // Delete the container that stores the HashNode
        delete container_of(node_to_be_deleted, struct Entry, node);
        emit_simple_string(buf, (uint8_t*)SIMPLE_STRING_OK, 2);
        return;
    }

    // If key is not in list, err
    emit_simple_error(buf, (uint8_t*)SIMPLE_ERROR_BAD, 3);
}

// TODO: remove?
// TODO: move this into utils
// curr_payload is a pointer to a location in the payload
// const uint8_t *&curr_payload is a reference to a pointer, essentially serves the same purpose as pointer to a pointer
// static int read_uint32(const uint8_t *&curr_payload, const uint8_t *end, uint32_t &target) {
//     if (curr_payload + 4 > end) {
//         return -1;
//     }

//     // actuall read into target now
//     memcpy(&target, curr_payload, 4);
//     curr_payload += 4;
//     return 0; 
// }

// static int read_str(const uint8_t *&curr_payload, uint32_t len, const uint8_t *end, std::string &target) {
//     if (curr_payload + len > end) {
//         return -1;
//     }

//     // actuall read into target now
//     target.assign(curr_payload, curr_payload + len);
//     curr_payload += len;
//     return 0; 
// }

// Pass payload and size for defensize programming in case user sent non matching payload size and actual payload size
// TODO: convert backt o const uint8_t *payload for best practice? to expose the least amount of data possible
static int parse_request(Buffer *buf, std::vector<std::string> &cmd) {
    // read tag
    printf("\nTrying to parse request!\n");
    Tags tag;
    if (read_tag(buf, tag) < 0 || tag != Tags::TAG_ARRAY) {
        return -1;
    }
    printf("tag value: %c\n", tag);

    // Read array length
    uint32_t nstr;
    if (read_array_length(buf, nstr) < 0) {
        return -1;
    }

    printf("Request parsing got nstr: %u\n", nstr);

    // Expecting to iterate nstr times
    for (uint32_t i = 0; i < nstr; i++) {
        // ensure tag is bulk string, otherwise error
        Tags bstr_tag;
        if (read_tag(buf, bstr_tag) < 0 || bstr_tag != Tags::TAG_BULK_STRING) {
            return -1;
        }

        // read len of string then push back onto cmd vector
        // read string into cmd
        cmd.push_back(std::string());
        if (read_bulk_string(buf, cmd.back()) < 0) {
            return -1;
        }
    }

    // TODO: how to add handle post read verification for RESP2 protocol?
    // if (payload != end) { // some how length header was bigger than payload provided
    //     return -1;
    // }

    return 0;
}

// support get, set, del for now
static void do_request(std::vector<std::string> &cmd, Buffer *buf) {
    // first match first cmd
    if (cmd.size() == 2 && cmd[0] == "get") {
        // NOTE: [] subscript syntax creates a new entry in the map, we use .find to check for existence
        hmap_do_get(&global_hmap, cmd, buf);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        hmap_do_set(&global_hmap, cmd, buf);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        hmap_do_del(&global_hmap, cmd, buf);
    } else {
        emit_simple_error(buf, (uint8_t*)SIMPLE_ERROR_CMD_NOT_FOUND, 13);
        return;
    }

    return;
}

// TODO: support new protocol of message payload => nstr | len | msg1 | len | msg2 | ...
// What to do?
// Read protocol header first (4 bytes) => can we read?
// try to read body => can we read?
// write result to data_out
static bool try_one_request(Conn *conn) {
    std::vector<std::string> cmd;
    if (parse_request(conn->data_in, cmd) < 0) { // TODO: migrate away from explicitly passing in Buffer*
        return false;
    }

    for (std::string &s : cmd) {
        printf("Got string: %s\n", s.c_str());
    }

    do_request(cmd, conn->data_out);

    // consume existing buffer data_in
    // buf_consume(conn->data_in, 4 + len);

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

    Conn *conn = new Conn(); // idiomatic C++ pointer instantiation
    conn->fd = client_fd;
    conn->want_read = true;
    conn->data_in = new Buffer();
    conn->data_out = new Buffer();

    return conn;
}

static void handle_write(Conn *conn) {
    // make sure data_out is non 0
    assert(conn->data_out->size() > 0);
    // write syscall
    ssize_t rv = write(conn->fd, conn->data_out->data(), conn->data_out->size());

    // handle case of full kernel buffer for pipelined requests
    if (rv < 0 && errno == EAGAIN) {
        return; // client socket is not ready to write to
    }

    if (rv < 0) { // write errored out
        conn->want_close = true;
        return;
    }

    buf_consume(conn->data_out, (size_t)rv); // consume rv sized buffer from the front

    // switch to read if we have written all data
    if (conn->data_out->size() == 0) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

static void handle_read(Conn *conn) {
    // NOTE: uint8_t is pretty much char
    uint8_t buf[64 * 1024];

    // NOTE: this can receive much more than just one requests worth of data
    // we can batch process, if there is enough data
    ssize_t rv = read(conn->fd, buf, sizeof(buf)); // rv is number of bytes received

    // we only handle_read when fd is ready to read so if we read nothing or errored, we just close socket connection
    if (rv <= 0) {
        conn->want_close = true;
        return;
    }

    // Append buffer to data_in buffer of connection
    buf_append(conn->data_in, buf, (size_t) rv);
    
    // Optimization: pipelined request handling => try to process until one fails
    while (try_one_request(conn)) {} // this will ever only append to data_out on successful protocol parsing

    // switch to write iff we have data to write, because we will only ever be in read or write mode
    if (conn->data_out->size() > 0) {
        conn->want_read = false;
        conn->want_write = true;

        handle_write(conn); // Optimization: optimisitic non-blocking write
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
                delete conn->data_in;
                delete conn->data_out;
                delete conn; // free pointer
            }
        }
    }
}