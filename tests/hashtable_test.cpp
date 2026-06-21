#include <gtest/gtest.h>
#include <hashtable.hpp>

struct Entry {
    HashNode node;
    std::string key;
    std::string value;
};

// FNV hash
// hash = (hash XOR byte) * prime, hash starts as a prime (?)
uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h ^ data[i]) * 0x01000193;
    }
    return h;
}

bool entry_eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

// NOTE: idempotence should be handled by user with lookup before doing an insertion
TEST(HashTableTest, InsertLoopUpWorks) {
    HashMap hmap = HashMap{};

    Entry entry;
    entry.key = "foo";
    entry.value = "hello";
    entry.node.hash = str_hash((uint8_t*)entry.key.data(), entry.key.length());
    
    hm_insert(&hmap, &entry.node);

    ASSERT_EQ(hmap.newer.size, 1);

    HashNode *node = hm_lookup(&hmap, &entry.node, &entry_eq);
    ASSERT_NE(node, nullptr);

    Entry* entry_container = container_of(node, struct Entry, node);
    ASSERT_EQ(entry_container->value, "hello");
}

TEST(HashTableTest, DeleteWorks) {
    HashMap hmap = HashMap{};

    Entry entry;
    entry.key = "foo";
    entry.value = "hello";
    entry.node.hash = str_hash((uint8_t*)entry.key.data(), entry.key.length());
    
    hm_insert(&hmap, &entry.node);
    EXPECT_EQ(hmap.newer.size, 1);

    HashNode *node = hm_delete(&hmap, &entry.node, &entry_eq);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(hmap.newer.size, 0);
}
