#ifndef VETH_LIST_H
#define VETH_LIST_H

struct veth_pair {
    char* veth1;
    char* veth2;
};

struct veth_list {
    struct veth_pair* veth_pairs;
    int* free_list;
    int size;
    int capacity;
};

#define IFNAMSIZ 16

extern struct veth_list* veths;
extern char phy_if[IFNAMSIZ];

// Create a veth_list struct
struct veth_list* veth_list_create(int capacity);

// Destroy a veth_list struct
void veth_list_destroy(struct veth_list* veth_list);

// Add a veth pair to the veth_list
int veth_list_add(struct veth_list* veth_list, const char* veth1, const char* veth2);

// Remove a veth pair from the veth_list
int veth_list_remove(struct veth_list* veth_list, const char* prefix);

// Get a veth pair from the veth_list
struct veth_pair* veth_list_get(const struct veth_list* veth_list, const int index);

// Print the veth_list
void veth_list_print(const struct veth_list* veth_list);

// Get the size of the veth_list
int veth_list_size(const struct veth_list* veth_list);

#endif  // VETH_LIST_H
