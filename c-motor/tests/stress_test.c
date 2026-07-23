#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../protocol.h"
#include "../database/db.h"
#include "../database/db_utils.h"
#define test_REPORT_FILENAME "tests/reports/stress_test_report.md"

#define TOTAL_ITERATIONS 1000

void get_process_memory(long *vm_rss, long *vm_hwm) {
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    *vm_rss = 0;
    *vm_hwm = 0;

    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %ld", vm_rss);
            } 
            else if (strncmp(line, "VmHWM:", 6) == 0) {
                sscanf(line, "VmHWM: %ld", vm_hwm);
            }
        }
        fclose(fp);
    }
}

void get_system_hardware(char *cpu_model, size_t cpu_sz, char *ram_total, size_t ram_sz) {
    FILE *fp;
    char line[256];

    strncpy(cpu_model, "Desconhecido", cpu_sz);
    strncpy(ram_total, "Desconhecido", ram_sz);

    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *info = strchr(line, ':');
                if (info) {
                    info += 2; 
                    info[strcspn(info, "\n")] = 0;
                    strncpy(cpu_model, info, cpu_sz - 1);
                    break;
                }
            }
        }
        fclose(fp);
    }

    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal", 8) == 0) {
                char *info = strchr(line, ':');
                if (info) {
                    info += 2;
                    
                    long long kb = atoll(info); 
                    double gb = (double)kb / (1024.0 * 1024.0);
                    
                    snprintf(ram_total, ram_sz, "%.2f GB", gb);
                    break;
                }
            }
        }
        fclose(fp);
    }
}

void run_tps_stress_test(void){

    struct timespec start, end;
    struct timespec cpu_start, cpu_end;
    int success_count = 0;
    int error_count = 0;
    user_protocol_t user;

    printf("Iniciando teste de stress...\n");
    printf("Alvo: %d inserções sequênciais.\n", TOTAL_ITERATIONS);

    long ram_start_rss, ram_start_hwm;
    long ram_end_rss, ram_end_hwm;

    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    get_process_memory(&ram_start_rss, &ram_start_hwm);

    for(int i = 0; i < TOTAL_ITERATIONS; i++){
        memset(&user, 0, sizeof(user));

        snprintf(user.full_name, sizeof(user.full_name), "Stress Test User %d", i);
        snprintf(user.email, sizeof(user.email), "stress_user_%d@domain.com", i);

        if(db_insert_user(&user) == DB_SUCCESS){
            success_count++;
        }else{
            error_count++;
        }

        if ((i + 1) % 1000 == 0) {
            printf("   -> Processados %d registos...\n", i + 1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
    get_process_memory(&ram_end_rss, &ram_end_hwm);

    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double cpu_time = (cpu_end.tv_sec - cpu_start.tv_sec) + (cpu_end.tv_nsec - cpu_start.tv_nsec) / 1e9;
    double tps = (total_time > 0) ? (success_count / total_time) : 0;
    long ram_diff_kb = ram_end_rss - ram_start_rss;

    printf("\n============================================================\n");
    printf("                  RELATORIO DE PERFORMANCE                  \n");
    printf("============================================================\n");
    printf("Tempo total (Clock)     : %.2f segundos\n", total_time);
    printf("Tempo dedicado pela CPU : %.2f segundos\n", cpu_time);
    printf("Uso de RAM (Início)     : %ld KB\n", ram_start_rss);
    printf("Uso de RAM (Fim)        : %ld KB\n", ram_end_rss);
    printf("Pico de RAM (HWM)       : %ld KB\n", ram_end_hwm);
    printf("Insercoes com sucesso   : %d\n", success_count);
    printf("Falhas de insercao      : %d\n", error_count);
    printf("Desempenho do Motor     : %.2f TPS (Transacoes/Segundo)\n", tps);
    printf("============================================================\n");

    char filename[64];
    int file_index = 1;
    
    while (1) {
        snprintf(filename, sizeof(filename), "tests/reports/stress_test_report_%d.md", file_index);
        if (access(filename, F_OK) == -1) {
            break; 
        }
        file_index++;
    }

    char cpu_info[128];
    char ram_info[64];
    get_system_hardware(cpu_info, sizeof(cpu_info), ram_info, sizeof(ram_info));

    FILE *test_report = fopen(filename, "w");
    if (test_report == NULL) {
        fprintf(stderr, "Erro ao criar o arquivo de relatório: %s\n", filename);
        return;
    }

    fprintf(test_report, "# Relatório de Performance - Motor C (Execução #%d)\n\n", file_index);
    fprintf(test_report, "### Especificações do Ambiente de Teste\n");
    fprintf(test_report, "* **Processador (CPU):** %s\n", cpu_info);
    fprintf(test_report, "* **Memória RAM Total:** %s\n\n", ram_info);

    fprintf(test_report, "### Consumo de Recursos do Processo\n\n");
    fprintf(test_report, "| Métrica de Recurso | Valor Obtido |\n");
    fprintf(test_report, "| :--- | :--- |\n");
    fprintf(test_report, "| **Tempo Real (Wall-clock)** | %.2f s |\n", total_time);
    fprintf(test_report, "| **Tempo Ativo de CPU** | %.2f s |\n", cpu_time);
    fprintf(test_report, "| **RAM Inicial (VmRSS)** | %ld KB |\n", ram_start_rss);
    fprintf(test_report, "| **RAM Final (VmRSS)** | %ld KB |\n", ram_end_rss);
    fprintf(test_report, "| **Pico de RAM (VmHWM)** | **%ld KB** |\n", ram_end_hwm);
    fprintf(test_report, "| **Crescimento de RAM** | %ld KB |\n\n", ram_diff_kb);

    fprintf(test_report, "### Resultados de Transação (MySQL)\n\n");
    fprintf(test_report, "| Métrica de Execução | Valor Obtido |\n");
    fprintf(test_report, "| :--- | :--- |\n");
    fprintf(test_report, "| **Iterações Planejadas** | %d |\n", TOTAL_ITERATIONS);
    fprintf(test_report, "| **Inserções com Sucesso** | %d |\n", success_count);
    fprintf(test_report, "| **Falhas Detectadas** | %d |\n", error_count);
    fprintf(test_report, "| **Vazão (Throughput)** | **%.2f TPS** |\n\n", tps);
    
    if (error_count == 0 && ram_diff_kb < 5000) {
        fprintf(test_report, "> **Status:** Teste concluído com sucesso operacional de 100%%. Memória aparenta estar estável.\n");
    } else if (error_count > 0) {
        fprintf(test_report, "> **Aviso:** Ocorreram falhas durante o processamento de banco de dados.\n");
    } else {
        fprintf(test_report, "> **Atenção:** Alto crescimento de RAM detectado no processo, verifique potenciais *memory leaks* (vazamentos).\n");
    }

    fclose(test_report);
    
    printf("\nRelatório salvo com sucesso em: %s\n", filename);
}