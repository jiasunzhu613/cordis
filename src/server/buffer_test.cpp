#include "utils.hpp"
#include "hashtable.hpp"

// offsetof is a C macro that finds the offset of a member in accordance to a struct in bytes
//    NOTE: the linux version of the container_of macro uses gcc macro extensions to do a member check 
// NOTE: we cast to char* because we want to make byte sized moves to the pointer mem address
// After we cast back to the struct type to allow for proper dereferencing/loading of bytes
#define container_of(ptr, T, member) \
    ((T *)( (char *)ptr - offsetof(T, member) ))

struct Entry {
    std::string key;
    std::string value;
    HashNode node;
};

static bool entry_eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

// TODO: update to gtests???
int main() {
    // Buffer *buf = new Buffer(50);

    // printf("new buffer size: %lu\n", buf->curr_size);

    // std::vector<uint8_t> input_buf(100, 10);
    // buf_append(buf, input_buf.data(), input_buf.size());

    // printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    // buf_consume(buf, 100);

    // printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    // printf("new buffer size: %lu\n", buf->curr_size);

    // buf_append(buf, input_buf.data(), input_buf.size());

    // printf("Length of data: %lu\n", buf->data_end - buf->data_begin);

    HashMap *hmap = new HashMap();
    Entry entry = Entry{.key="hello", .value="world"};
    HashNode node = HashNode{.next=NULL, .hash=1};
    entry.node = node;

    printf("new tab: %p\n", hmap->newer.tab);
    hm_insert(hmap, &entry.node);
    printf("Mem address stored at new tab ind 1 entry: %p, node: %p\n", hmap->newer.tab[1], &entry.node);

    // make sure to be using entry.node here!
    HashNode *got_node = hm_lookup(hmap, &entry.node, &entry_eq);

    Entry *ent = container_of(got_node, Entry, node);
    printf("%p, key: %s, value: %s\n", got_node, ent->key.c_str(), ent->value.c_str());
    return 0;
}