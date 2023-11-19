#pragma once

#define VETH_NUM 100
#include <stdbool.h>

bool create_veth(char* veth_name);
bool delete_veth(char* veth_name);
int add_to_veth_list(char* veth_name);
int remove_from_veth_list(char* veth_name);
char** get_veth_list();
int init_veth_list();