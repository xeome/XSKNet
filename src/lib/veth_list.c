#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>

#include "args.h"
#include "lwlog.h"
#include "veth_list.h"
#include "uthash.h"

struct veth_pair* veths = NULL;

int veth_list_add(struct veth_pair** veth_map, const char* prefix) {
    struct veth_pair* veth_pair = NULL;
    HASH_FIND_STR(*veth_map, prefix, veth_pair);
    if (veth_pair != NULL) {
        lwlog_err("veth pair with prefix %s already exists", prefix);
        return -1;  // Return without freeing if the entry already exists
    }

    struct veth_pair* new_entry = (struct veth_pair*)malloc(sizeof(struct veth_pair));
    if (new_entry == NULL) {
        lwlog_err("malloc: %s", strerror(errno));
        return -1;
    }

    char veth1[IFNAMSIZ];
    char veth2[IFNAMSIZ];
    snprintf(veth1, IFNAMSIZ, "%s_inner", prefix);
    snprintf(veth2, IFNAMSIZ, "%s_outer", prefix);

    strncpy(new_entry->prefix, prefix, sizeof(new_entry->prefix) - 1);
    new_entry->prefix[sizeof(new_entry->prefix) - 1] = '\0';  // Ensure null-termination

    new_entry->veth1 = strdup(veth1);
    new_entry->veth2 = strdup(veth2);

    HASH_ADD_STR(*veth_map, prefix, new_entry);
    veth_list_print(*veth_map);
    return 0;
}

int veth_list_remove(struct veth_pair** veth_map, const char* prefix) {
    struct veth_pair* veth_pair = NULL;
    veth_list_print(*veth_map);
    HASH_FIND_STR(*veth_map, prefix, veth_pair);
    if (veth_pair == NULL) {
        lwlog_err("veth pair with prefix %s does not exist", prefix);
        return -1;
    }

    HASH_DEL(*veth_map, veth_pair);
    free(veth_pair);

    // Set the pointer to NULL to avoid use-after-free
    veth_pair = NULL;

    return 0;
}

void veth_list_print(struct veth_pair* veth_map) {
    struct veth_pair *current, *tmp;
    lwlog_info("Dumping veth_map");
    HASH_ITER(hh, veth_map, current, tmp) {
        lwlog_info("veth prefix: %s, veth1: %s, veth2: %s", current->prefix, current->veth1, current->veth2);
    }
}

void veth_list_destroy(struct veth_pair* veth_map) {
    struct veth_pair *current, *tmp;
    veth_list_print(veth_map);
    HASH_ITER(hh, veth_map, current, tmp) {
        HASH_DEL(veth_map, current);
        free(current);
    }
}