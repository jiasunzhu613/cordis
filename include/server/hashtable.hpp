#pragma once

#include <assert.h>
#include <cstdlib>

#include "utils.hpp"

// Define intrusive linkedlist data structure for separate chaining
struct HashNode {
    HashNode *next;
    uint64_t hash;
};

struct HashTab {
    // Elements are assigned based on % size
    HashNode **tab;

    // bitmask used to quickly calculate % size
    // instead of % size, we can do & (size - 1)
    size_t mask; // stores size - 1
    size_t size;
};


// Consolidated hash map structure that is used for querying
// has old and new table for migrating to larger tables iteratively
struct HashMap {
    HashTab older;
    HashTab newer;

    size_t migrate_pos;
};

// Function definitions
HashNode *hm_lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *)); // eq is a function pointer
void hm_insert(HashMap *hmap, HashNode *Node);
HashNode *hm_delete(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *));
void hm_clear(HashMap *hmap);
size_t hm_size(HashMap *hmap);