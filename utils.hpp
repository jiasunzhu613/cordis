#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include <vector>

const int MAX_MSG_SIZE = 32 << 20;

// TODO: support void *
int read_all(int fd, char *buf, size_t n);
int write_all(int fd, char *buf, size_t n);

// NOTE: vectors are passed by value (copied entirely) by default in cpp
void buf_append(std::vector<uint8_t> &vec, const uint8_t *data, size_t len);
void buf_consume(std::vector<uint8_t> &vec, size_t n); // consume n from the start
