#include "hashtable.hpp"

/*
Hashmap architecture:
HashMap => 2 HashTables
    => one old 
    => one new for iterative resizing

HashTable => separate chaining fixed-sidez hashtable
*/
#define container_of(ptr, T, member) \
    ((T *)( (char *)ptr - offsetof(T, member) ))

const size_t k_max_load_factor = 8;
const size_t k_rehashing_constant_work = 128;

// TODO: init individual hash tables
static void htab_init(HashTab *htab, size_t size) {
    assert(size > 0 && (size & (size - 1)) == 0); // check that size is a power of 2
    htab->tab = (HashNode**) calloc(size, sizeof(HashNode*));
    htab->mask = size - 1;
    htab->size = 0; // number of nodes in htab
}

static void htab_insert(HashTab *htab, HashNode *node) {
    size_t ind = node->hash & htab->mask; // calculate hash % size using bitwise and
    HashNode *p = htab->tab[ind];
    node->next = p;
    htab->tab[ind] = node;
    htab->size++;
}

// We return HashNode** because it helps us with being able to delete nodes along with inserting new nodes
static HashNode **htab_lookup(HashTab *htab, HashNode *node, bool (*eq)(HashNode *, HashNode *)) {
    if (!htab->tab) {
        return NULL;
    }

    size_t ind = node->hash & htab->mask; 

    HashNode **from = &htab->tab[ind]; // pointer to the target
    HashNode *cur;
    while ((cur = *from) != NULL) {
        if (cur->hash == node->hash && eq(cur, node)) {
            return from;
        }

        from = &cur->next;
    }

    return NULL;
}

// Hashtable deletion
// By having htab_lookup return a pointer to a pointer, it simplifies a lot of cases for delete:
//  e.g case of deleting the first node and having to update the array slot
//      => this is handled by updating the memory address the pointer points to itself
static HashNode *htab_detach(HashTab *htab, HashNode **from) {
    HashNode *node = *from; // node we want to detach
    *from = node->next; // update the incoming pointer to node structure that we want to detach
    htab->size--;
    return node; // return the detached node
}

// Hashmap function

// Move new to old and create new HashTab for new with larger size
static void hm_triger_resize(HashMap *hmap) {
    hmap->older = hmap->newer;
    htab_init(&hmap->newer, (hmap->newer.mask + 1) * 2); // maintain power of 2
    hmap->migrate_pos = 0;
}

static void hm_help_rehashing(HashMap *hmap) {
    // iteratively move through the old table from start to end doing constant amount of work
    size_t work_done = 0;
    while (work_done < k_rehashing_constant_work && hmap->older.size > 0) {
        HashNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from) {
            // move migrate_pos pointer forward
            hmap->migrate_pos++;
            continue;
        }

        // remove from hashmap chain and insert into new table
        htab_insert(&hmap->newer, htab_detach(&hmap->older, from));
        work_done++;
    }

    // if old hashmap is empty, rehashing process is done
    if (hmap->older.size == 0 && hmap->older.tab) {
        free(hmap->older.tab); // free allocated memory
        hmap->older = HashTab{};
    }
}

HashNode *hm_lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    // search new table first
    HashNode **from = htab_lookup(&hmap->newer, key, eq);

    // if not in new table, check old table
    if (!from) {
        from = htab_lookup(&hmap->older, key, eq);
    }

    return from ? *from : NULL;
}

void hm_insert(HashMap *hmap, HashNode *node) {
    // init new table if empty
    if (!hmap->newer.tab) {
        htab_init(&hmap->newer, 4);
    } 

    // insert into table
    htab_insert(&hmap->newer, node);

    // Check if we need to trigger resizing
    if (!hmap->older.tab) {
        size_t threshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= threshold) {
            hm_triger_resize(hmap);
        }
    }
    hm_help_rehashing(hmap); // migrate some keys
}

HashNode *hm_delete(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    // search new table first
    if (HashNode **from = htab_lookup(&hmap->newer, key, eq)) {
        return htab_detach(&hmap->newer, from);
    }

    // check old table otherwise
    if (HashNode **from = htab_lookup(&hmap->older, key, eq)) {
        return htab_detach(&hmap->older, from);
    }

    return NULL;
}
