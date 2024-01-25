#pragma once
#include <signal.h>
void daemon_signal_init();
void client_signal_init();
void exit_daemon();
void exit_client();

extern int global_exit_flag;