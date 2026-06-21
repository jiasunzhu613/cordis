#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <sstream>
#include <set>
#include <iterator>

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

void print_cordis_response(Cordis_Response response) {
    switch(response.tag) {
    case TAG_NULL: 
        printf("Tag type: %c, response: %s\n", response.tag, "<null type has no response>");
        break;
    case TAG_SIMPLE_STRING:
        printf("Tag type: %c, response: %s\n", response.tag, response.simple_string.c_str());
        break;
    case TAG_SIMPLE_ERROR:
        printf("Tag type: %c, response: %s\n", response.tag, response.simple_error.c_str());
        break;
    case TAG_INT64:
        printf("Tag type: %c, response: %ld\n", response.tag, response.integer);
        break;
    case TAG_BULK_STRING:
        printf("Tag type: %c, response: %s\n", response.tag, response.bulk_string.c_str());
        break;
    case TAG_ARRAY:
        printf("Tag type: %c\n", response.tag);
        for (Cordis_Response elem : response.array) {
            print_cordis_response(elem);
        }
        printf("END array Cordis_Response\n");
        break;
    }
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

    // Supported operations
    std::set<std::string> operations = {"get", "set", "del", "keys"};

    // REPL client CLI
    while (1) {
        // Read user input, line by line, make sure to match against valid, supported keys
        std::string line;
        std::getline(std::cin, line);
        std::istringstream iss(line);

        // Stream iterators construct the vector automatically
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                        std::istream_iterator<std::string>{}};

        if (tokens.size() < 1 || tokens[0] == "exit") {
            break;
        }

        if (!operations.count(tokens[0])) {
            std::cout << "unsupported operation of: " << tokens[0] << std::endl;
            continue;
        }

        // Process response
        Buffer buf;
        int err = send_req(fd, tokens, tokens.size());
        if (err) {
            close(fd);
            return 1;
        }
        err = read_data(fd, &buf);
        if (err) {
            close(fd);
            return 1;
        }

        Cordis_Response response;
        int cordis_err = read_one(&buf, &response);
        if (cordis_err < 0) {
            close(fd);
            return 1;
        }


        print_cordis_response(response);
    }

    return 0;
}