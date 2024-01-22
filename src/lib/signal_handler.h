#pragma once
#include <signal.h>
void daemon_signal_init();
void client_signal_init();
void exit_daemon();

extern volatile sig_atomic_t global_exit_flag;