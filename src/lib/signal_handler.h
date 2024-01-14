#pragma once
#include <signal.h>
void signal_init();
void exit_daemon();

extern volatile sig_atomic_t global_exit_flag;