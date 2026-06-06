#include "utils.hpp"

// TODO: update to gtests???
int main() {
    Buffer *buf = new Buffer(50);

    printf("new buffer size: %lu\n", buf->curr_size);

    std::vector<uint8_t> input_buf(100, 10);
    buf_append(buf, input_buf.data(), input_buf.size());

    printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    buf_consume(buf, 100);

    printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    printf("new buffer size: %lu\n", buf->curr_size);

    buf_append(buf, input_buf.data(), input_buf.size());

    printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    return 0;
}