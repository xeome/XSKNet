#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwlog.h"
#include "veth_list.h"

struct veth_list* veths;

struct veth_list* veth_list_create(const int capacity) {
    struct veth_list* veth_list = malloc(sizeof(struct veth_list));
    veth_list->veth_pairs = malloc(sizeof(struct veth_pair) * capacity);
    veth_list->free_list = malloc(sizeof(int) * capacity);
    veth_list->size = 0;
    veth_list->capacity = capacity;
    for (int i = 0; i < capacity; i++) {
        veth_list->free_list[i] = i;
    }
    return veth_list;
}

void veth_list_destroy(struct veth_list* veth_list) {
    free(veth_list->veth_pairs);
    free(veth_list->free_list);
    free(veth_list);
}

int veth_list_add(struct veth_list* veth_list, const char* veth1, const char* veth2) {
    if (veth_list == NULL) {
        lwlog_err("veth_list is NULL");
        return -1;
    }

    if (veth_list->size == veth_list->capacity) {
        return -1;
    }
    const int index = veth_list->free_list[veth_list->size];
    veth_list->veth_pairs[index].veth1 = strdup(veth1);
    veth_list->veth_pairs[index].veth2 = strdup(veth2);
    veth_list->size++;
    return index;
}

int veth_list_remove(struct veth_list* veth_list, const char* prefix) {
    for (int i = 0; i < veth_list->size; i++) {
        if (strncmp(veth_list->veth_pairs[i].veth1, prefix, strlen(prefix)) == 0) {
            free(veth_list->veth_pairs[i].veth1);
            free(veth_list->veth_pairs[i].veth2);
            veth_list->veth_pairs[i].veth1 = NULL;
            veth_list->veth_pairs[i].veth2 = NULL;
            veth_list->free_list[veth_list->size] = i;
            veth_list->size--;
            return i;
        }
    }
    lwlog_err("Failed to remove veth pair: %s", prefix);
    return -1;
}

struct veth_pair* veth_list_get(const struct veth_list* veth_list, const int index) {
    if (index < 0 || index >= veth_list->size) {
        return NULL;
    }
    return &veth_list->veth_pairs[index];
}

void veth_list_print(const struct veth_list* veth_list) {
    printf("veth_list: size=%d capacity=%d\n", veth_list->size, veth_list->capacity);
    for (int i = 0; i < veth_list->size; i++) {
        printf("  %d: %s %s\n", i, veth_list->veth_pairs[i].veth1, veth_list->veth_pairs[i].veth2);
    }
    for (int i = veth_list->size; i < veth_list->capacity; i++) {
        printf("  %d: free\n", i);
    }
}

int veth_list_size(const struct veth_list* veth_list) {
    return veth_list->size;
}
