#pragma once
#include "uthash.h"

struct veth_pair {
    char prefix[16];  // key for the hash table
    // const char* prefix;
    char* veth1;
    char* veth2;
    UT_hash_handle hh;  // makes this structure hashable
};

extern char phy_if[16];
extern struct veth_pair* veths;

// Create a veth_list struct
// struct veth_list* veth_list_create(int capacity);

// Destroy a veth_list struct
void veth_list_destroy(struct veth_pair* veth_map);

// Add a veth pair to the veth_list
int veth_list_add(struct veth_pair** veth_map, const char* prefix);

// Remove a veth pair from the veth_list
int veth_list_remove(struct veth_pair** veth_map, const char* prefix);

// Print the veth_list
void veth_list_print(struct veth_pair* veth_map);