#ifndef STRESS_TEST_H
#define STRESS_TEST_H

#include <time.h>
#include <unistd.h>

void get_system_hardware(char *cpu_model, size_t cpu_sz, char *ram_total, size_t ram_sz);
void run_tps_stress_test(void);

#endif