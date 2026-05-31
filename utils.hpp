#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <assert.h>

#define MAX_MSG_SIZE 4096

// TODO: support void *
int read_all(int fd, char *buf, size_t n);
int write_all(int fd, char *buf, size_t n);